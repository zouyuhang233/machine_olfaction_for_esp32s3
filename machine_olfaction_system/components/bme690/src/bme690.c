/*
 * bme690.c -- BME690 的 ESP-IDF I²C 适配层
 *
 * 设计原则：
 *   - 不自己重写 Bosch 的补偿/加热算法，全部用官方 BME69X SensorAPI；
 *   - 本文件只负责总线适配、默认配置、单步测量与多步剖面采集；
 *   - 所有返回值都转成 esp_err_t，日志里带底层返回码，方便定位是总线问题还是芯片问题。
 */
#include "bme690.h"

#include <string.h>

#include "bme69x.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "bme690";

#define BME690_I2C_TIMEOUT_MS 100
#define BME690_DEFAULT_TEMP_C 300
#define BME690_DEFAULT_DUR_MS 100

typedef struct {
    i2c_port_t port;
    uint8_t addr;
} bme690_bus_t;

static bme690_bus_t s_bus;
static struct bme69x_dev s_dev;
static struct bme69x_conf s_conf;
static struct bme69x_heatr_conf s_heatr_forced;
static struct bme69x_heatr_conf s_heatr_parallel;

static bool s_ready;
static int8_t s_last_rslt;
static uint16_t s_heatr_temp = BME690_DEFAULT_TEMP_C;
static uint16_t s_heatr_dur = BME690_DEFAULT_DUR_MS;

static uint16_t s_profile_temp[10];
static uint16_t s_profile_dur[10];
static uint8_t s_profile_len;
static uint16_t s_profile_shared_dur;

/* ---------------- ESP-IDF I²C → Bosch API 函数指针 ---------------- */

static BME69X_INTF_RET_TYPE bme69x_i2c_read(uint8_t reg_addr, uint8_t *reg_data,
                                             uint32_t length, void *intf_ptr)
{
    const bme690_bus_t *bus = (const bme690_bus_t *)intf_ptr;
    esp_err_t err = i2c_master_write_read_device(bus->port, bus->addr, &reg_addr, 1,
                                                 reg_data, length,
                                                 pdMS_TO_TICKS(BME690_I2C_TIMEOUT_MS));
    return (err == ESP_OK) ? BME69X_INTF_RET_SUCCESS : (BME69X_INTF_RET_TYPE)-1;
}

static BME69X_INTF_RET_TYPE bme69x_i2c_write(uint8_t reg_addr, const uint8_t *reg_data,
                                              uint32_t length, void *intf_ptr)
{
    const bme690_bus_t *bus = (const bme690_bus_t *)intf_ptr;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (cmd == NULL) {
        return (BME69X_INTF_RET_TYPE)-1;
    }

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (uint8_t)((bus->addr << 1) | I2C_MASTER_WRITE), true);
    i2c_master_write_byte(cmd, reg_addr, true);
    if (length > 0) {
        i2c_master_write(cmd, reg_data, length, true);
    }
    i2c_master_stop(cmd);

    esp_err_t err = i2c_master_cmd_begin(bus->port, cmd,
                                         pdMS_TO_TICKS(BME690_I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);
    return (err == ESP_OK) ? BME69X_INTF_RET_SUCCESS : (BME69X_INTF_RET_TYPE)-1;
}

static void bme69x_delay_us(uint32_t period, void *intf_ptr)
{
    (void)intf_ptr;
    if (period == 0) {
        return;
    }

    /* 长时间等待让出 CPU；最后不足 1ms 的部分用忙等保证精度。 */
    if (period >= 1000) {
        vTaskDelay(pdMS_TO_TICKS(period / 1000));
        period %= 1000;
    }
    if (period > 0) {
        esp_rom_delay_us(period);
    }
}

/* ---------------- 错误码转换 ---------------- */

static esp_err_t to_esp_err(int8_t rslt)
{
    s_last_rslt = rslt;
    switch (rslt) {
    case BME69X_OK:
        return ESP_OK;
    case BME69X_E_DEV_NOT_FOUND:
        return ESP_ERR_NOT_FOUND;
    case BME69X_W_NO_NEW_DATA:
        return ESP_ERR_TIMEOUT;
    case BME69X_E_NULL_PTR:
    case BME69X_E_COM_FAIL:
    case BME69X_E_INVALID_LENGTH:
    default:
        return ESP_FAIL;
    }
}

const char *bme690_rslt_name(int8_t rslt)
{
    switch (rslt) {
    case BME69X_OK:                     return "OK";
    case BME69X_E_NULL_PTR:             return "E_NULL_PTR";
    case BME69X_E_COM_FAIL:             return "E_COM_FAIL(I2C?)";
    case BME69X_E_DEV_NOT_FOUND:        return "E_DEV_NOT_FOUND(chip_id != 0x61)";
    case BME69X_E_INVALID_LENGTH:       return "E_INVALID_LENGTH";
    case BME69X_E_SELF_TEST:            return "E_SELF_TEST";
    case BME69X_W_DEFINE_OP_MODE:       return "W_DEFINE_OP_MODE";
    case BME69X_W_NO_NEW_DATA:          return "W_NO_NEW_DATA";
    case BME69X_W_DEFINE_SHD_HEATR_DUR: return "W_DEFINE_SHD_HEATR_DUR";
    default:                            return "UNKNOWN";
    }
}

/* ---------------- 初始化 ---------------- */

esp_err_t bme690_init(i2c_port_t port, gpio_num_t sda_io, gpio_num_t scl_io, uint8_t i2c_addr)
{
    if (s_ready) {
        return ESP_OK;
    }

    i2c_config_t cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda_io,
        .scl_io_num = scl_io,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,   /* 内部弱上拉只作兜底，外部 4.7k 不能省 */
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,            /* 调试先从 100kHz 开始 */
    };

    esp_err_t err = i2c_param_config(port, &cfg);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "i2c_param_config 失败: %s", esp_err_to_name(err));
        return err;
    }

    err = i2c_driver_install(port, I2C_MODE_MASTER, 0, 0, 0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "i2c_driver_install 失败: %s", esp_err_to_name(err));
        return err;
    }

    s_bus.port = port;
    s_bus.addr = i2c_addr;

    memset(&s_dev, 0, sizeof(s_dev));
    s_dev.intf = BME69X_I2C_INTF;
    s_dev.intf_ptr = &s_bus;
    s_dev.read = bme69x_i2c_read;
    s_dev.write = bme69x_i2c_write;
    s_dev.delay_us = bme69x_delay_us;
    s_dev.amb_temp = 25;

    int8_t rslt = bme69x_init(&s_dev);
    s_last_rslt = rslt;
    if (rslt != BME69X_OK) {
        ESP_LOGE(TAG, "bme69x_init 失败: %s (%d)", bme690_rslt_name(rslt), rslt);
        ESP_LOGE(TAG, "检查：SDA=GPIO%d SCL=GPIO%d 地址=0x%02X，CSB 是否直连 VDDIO，SDO 是否接 GND",
                 sda_io, scl_io, i2c_addr);

        /* 即使初始化失败，也尽量把原始 ID 读出来：0x61/0x02 是验货的关键。 */
        uint8_t raw_chip = 0, raw_variant = 0;
        if (bme69x_get_regs(BME69X_REG_CHIP_ID, &raw_chip, 1, &s_dev) == BME69X_OK &&
            bme69x_get_regs(BME69X_REG_VARIANT_ID, &raw_variant, 1, &s_dev) == BME69X_OK) {
            ESP_LOGE(TAG, "raw id: chip_id=0x%02X variant_id=0x%02X（BME690 应为 0x61 / 0x02）",
                     raw_chip, raw_variant);
        }
        return to_esp_err(rslt);
    }

    ESP_LOGI(TAG, "BME690 初始化成功: chip_id=0x%02X variant_id=0x%02X (BME690 应为 0x02)",
             s_dev.chip_id, (unsigned)s_dev.variant_id);

    s_conf.os_hum = BME69X_OS_1X;
    s_conf.os_temp = BME69X_OS_2X;
    s_conf.os_pres = BME69X_OS_16X;
    s_conf.filter = BME69X_FILTER_OFF;
    s_conf.odr = BME69X_ODR_NONE;
    rslt = bme69x_set_conf(&s_conf, &s_dev);
    if (rslt != BME69X_OK) {
        ESP_LOGE(TAG, "bme69x_set_conf 失败: %s (%d)", bme690_rslt_name(rslt), rslt);
        return to_esp_err(rslt);
    }

    memset(&s_heatr_forced, 0, sizeof(s_heatr_forced));
    s_heatr_forced.enable = BME69X_ENABLE;
    s_heatr_forced.heatr_temp = s_heatr_temp;
    s_heatr_forced.heatr_dur = s_heatr_dur;
    rslt = bme69x_set_heatr_conf(BME69X_FORCED_MODE, &s_heatr_forced, &s_dev);
    if (rslt != BME69X_OK) {
        ESP_LOGE(TAG, "设置加热配置失败: %s (%d)", bme690_rslt_name(rslt), rslt);
        return to_esp_err(rslt);
    }

    s_ready = true;
    ESP_LOGI(TAG, "默认配置: os_hum=1x os_temp=2x os_pres=16x, 加热 %u°C / %u ms",
             s_heatr_temp, s_heatr_dur);
    return ESP_OK;
}

bool bme690_ready(void)
{
    return s_ready;
}

int8_t bme690_last_rslt(void)
{
    return s_last_rslt;
}

uint8_t bme690_chip_id(void)
{
    return s_dev.chip_id;
}

uint8_t bme690_variant_id(void)
{
    return (uint8_t)s_dev.variant_id;
}

esp_err_t bme690_log_registers(void)
{
    if (!s_ready) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t ctrl[5] = {0};  /* 0x71 ctrl_gas_1, 0x72 ctrl_hum, 0x73 status, 0x74 ctrl_meas, 0x75 config */
    int8_t rslt = bme69x_get_regs(BME69X_REG_CTRL_GAS_1, ctrl, sizeof(ctrl), &s_dev);
    if (rslt != BME69X_OK) {
        return to_esp_err(rslt);
    }

    ESP_LOGI(TAG, "regs: 0x71=0x%02X 0x72=0x%02X 0x73=0x%02X 0x74=0x%02X 0x75=0x%02X",
             ctrl[0], ctrl[1], ctrl[2], ctrl[3], ctrl[4]);
    ESP_LOGI(TAG, "id: chip=0x%02X variant=0x%02X", s_dev.chip_id, (unsigned)s_dev.variant_id);
    return ESP_OK;
}

esp_err_t bme690_set_heater(uint16_t temp_c, uint16_t duration_ms)
{
    if (!s_ready) {
        return ESP_ERR_INVALID_STATE;
    }
    if (temp_c < 200 || temp_c > 400 || duration_ms < 1 || duration_ms > 4032) {
        ESP_LOGE(TAG, "加热参数越界: %u°C / %u ms（温度 200-400，时长 1-4032）", temp_c, duration_ms);
        return ESP_ERR_INVALID_ARG;
    }

    s_heatr_temp = temp_c;
    s_heatr_dur = duration_ms;
    s_heatr_forced.heatr_temp = temp_c;
    s_heatr_forced.heatr_dur = duration_ms;

    int8_t rslt = bme69x_set_heatr_conf(BME69X_FORCED_MODE, &s_heatr_forced, &s_dev);
    if (rslt != BME69X_OK) {
        ESP_LOGE(TAG, "bme69x_set_heatr_conf 失败: %s (%d)", bme690_rslt_name(rslt), rslt);
        return to_esp_err(rslt);
    }
    ESP_LOGI(TAG, "加热配置更新: %u°C / %u ms", temp_c, duration_ms);
    return ESP_OK;
}

/* ---------------- 单步强制模式测量 ---------------- */

esp_err_t bme690_read(bme690_reading_t *out)
{
    if (!s_ready) {
        return ESP_ERR_INVALID_STATE;
    }
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* 每次强制测量前重写一次单步配置：防止上一次并行剖面把 nb_conv 改成 0..9 的其他值。 */
    int8_t rslt = bme69x_set_heatr_conf(BME69X_FORCED_MODE, &s_heatr_forced, &s_dev);
    if (rslt != BME69X_OK) {
        return to_esp_err(rslt);
    }

    rslt = bme69x_set_op_mode(BME69X_FORCED_MODE, &s_dev);
    if (rslt != BME69X_OK) {
        return to_esp_err(rslt);
    }

    uint32_t duration_us = bme69x_get_meas_dur(BME69X_FORCED_MODE, &s_conf, &s_dev)
                           + (uint32_t)s_heatr_dur * 1000U;
    s_dev.delay_us(duration_us, s_dev.intf_ptr);

    struct bme69x_data data = {0};
    uint8_t n_fields = 0;
    rslt = bme69x_get_data(BME69X_FORCED_MODE, &data, &n_fields, &s_dev);
    if (rslt == BME69X_W_NO_NEW_DATA) {
        /* 首次测量偶发没准备好：再等 20ms 读一次。 */
        s_dev.delay_us(20000, s_dev.intf_ptr);
        rslt = bme69x_get_data(BME69X_FORCED_MODE, &data, &n_fields, &s_dev);
    }
    if (rslt != BME69X_OK) {
        return to_esp_err(rslt);
    }

    out->temperature_c = (float)data.temperature / 100.0f;
    out->pressure_pa = (float)data.pressure;
    out->humidity_pct = (float)data.humidity / 1000.0f;
    out->gas_resistance_ohm = (float)data.gas_resistance;
    out->status = data.status;
    out->gas_index = data.gas_index;

    /* 把实测环境温度用于下一次 res_heat_x 换算（手册 p27 的 amb_temp）。 */
    int amb = (int)(out->temperature_c >= 0.0f ? out->temperature_c + 0.5f
                                               : out->temperature_c - 0.5f);
    if (amb < -40) {
        amb = -40;
    } else if (amb > 85) {
        amb = 85;
    }
    s_dev.amb_temp = (int8_t)amb;
    return ESP_OK;
}

/* ---------------- 多步加热剖面（并行模式） ---------------- */

esp_err_t bme690_set_profile(const uint16_t *temps_c, const uint16_t *durations_ms,
                             uint8_t count, uint16_t shared_duration_ms)
{
    if (!s_ready) {
        return ESP_ERR_INVALID_STATE;
    }
    if (temps_c == NULL || durations_ms == NULL || count == 0 || count > 10) {
        return ESP_ERR_INVALID_ARG;
    }

    for (uint8_t i = 0; i < count; i++) {
        if (temps_c[i] < 200 || temps_c[i] > 400) {
            ESP_LOGE(TAG, "第 %u 步温度 %u°C 越界（200-400）", i, temps_c[i]);
            return ESP_ERR_INVALID_ARG;
        }
        if (durations_ms[i] == 0 || durations_ms[i] > 4032) {
            ESP_LOGE(TAG, "第 %u 步时长 %u ms 越界（1-4032）", i, durations_ms[i]);
            return ESP_ERR_INVALID_ARG;
        }
        s_profile_temp[i] = temps_c[i];
        s_profile_dur[i] = durations_ms[i];
    }
    s_profile_len = count;

    if (shared_duration_ms == 0) {
        uint32_t meas_ms = bme69x_get_meas_dur(BME69X_PARALLEL_MODE, &s_conf, &s_dev) / 1000U;
        s_profile_shared_dur = (meas_ms < 140U) ? (uint16_t)(140U - meas_ms) : 1U;
    } else {
        s_profile_shared_dur = shared_duration_ms;
    }

    memset(&s_heatr_parallel, 0, sizeof(s_heatr_parallel));
    s_heatr_parallel.enable = BME69X_ENABLE;
    s_heatr_parallel.heatr_temp_prof = s_profile_temp;
    s_heatr_parallel.heatr_dur_prof = s_profile_dur;
    s_heatr_parallel.profile_len = count;
    s_heatr_parallel.shared_heatr_dur = s_profile_shared_dur;

    int8_t rslt = bme69x_set_heatr_conf(BME69X_PARALLEL_MODE, &s_heatr_parallel, &s_dev);
    if (rslt != BME69X_OK) {
        ESP_LOGE(TAG, "设置加热剖面失败: %s (%d)", bme690_rslt_name(rslt), rslt);
        return to_esp_err(rslt);
    }

    ESP_LOGI(TAG, "加热剖面已设置: %u 步, shared=%u ms", count, s_profile_shared_dur);
    return ESP_OK;
}

esp_err_t bme690_read_profile(bme690_reading_t *out, uint8_t max_count, uint8_t *out_count)
{
    if (!s_ready) {
        return ESP_ERR_INVALID_STATE;
    }
    if (out == NULL || out_count == NULL || max_count < s_profile_len || s_profile_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(out, 0, (size_t)max_count * sizeof(*out));
    *out_count = 0;

    /* 重写并行配置：如果中间做过 bme690_read()，nb_conv 会被改成 0，必须先恢复到剖面步数。 */
    int8_t rslt = bme69x_set_heatr_conf(BME69X_PARALLEL_MODE, &s_heatr_parallel, &s_dev);
    if (rslt != BME69X_OK) {
        return to_esp_err(rslt);
    }

    rslt = bme69x_set_op_mode(BME69X_PARALLEL_MODE, &s_dev);
    if (rslt != BME69X_OK) {
        return to_esp_err(rslt);
    }

    uint32_t cycle_us = bme69x_get_meas_dur(BME69X_PARALLEL_MODE, &s_conf, &s_dev)
                        + (uint32_t)s_profile_shared_dur * 1000U;
    uint16_t seen = 0;
    uint8_t total = 0;

    /* 最多等 3 倍理论周期；正常情况一个周期能拿到 1-3 个 field。 */
    for (uint32_t attempt = 0; attempt < (uint32_t)s_profile_len * 3U + 6U; attempt++) {
        s_dev.delay_us(cycle_us, s_dev.intf_ptr);

        struct bme69x_data data[3] = {{0}};
        uint8_t n_fields = 0;
        rslt = bme69x_get_data(BME69X_PARALLEL_MODE, data, &n_fields, &s_dev);
        if (rslt == BME69X_W_NO_NEW_DATA) {
            continue;
        }
        if (rslt != BME69X_OK) {
            break;
        }

        for (uint8_t i = 0; i < n_fields; i++) {
            uint8_t idx = data[i].gas_index;
            if (idx >= s_profile_len || (seen & (1U << idx)) ||
                !(data[i].status & BME69X_NEW_DATA_MSK)) {
                continue;
            }
            out[idx].temperature_c = (float)data[i].temperature / 100.0f;
            out[idx].pressure_pa = (float)data[i].pressure;
            out[idx].humidity_pct = (float)data[i].humidity / 1000.0f;
            out[idx].gas_resistance_ohm = (float)data[i].gas_resistance;
            out[idx].status = data[i].status;
            out[idx].gas_index = data[i].gas_index;
            seen |= (uint16_t)(1U << idx);
            total++;
        }
        if (total >= s_profile_len) {
            break;
        }
    }

    (void)bme69x_set_op_mode(BME69X_SLEEP_MODE, &s_dev);
    *out_count = total;

    if (total < s_profile_len) {
        ESP_LOGW(TAG, "剖面只采到 %u/%u 步", total, s_profile_len);
        return to_esp_err(BME69X_W_NO_NEW_DATA);
    }
    return ESP_OK;
}
