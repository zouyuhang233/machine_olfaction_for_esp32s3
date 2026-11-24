/*
 * bme690.c -- BME690 I²C 适配层
 *
 * Bosch BME690 SensorAPI v1.1.0（BSD-3-Clause）负责补偿和寄存器细节；
 * 本文件只做 ESP-IDF I²C、身份检查、forced 模式调度和单位转换。
 */
#include "bme690.h"

#include <math.h>
#include <string.h>

#include "bme69x.h"
#include "board_pins.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "bme690";
static const i2c_port_t I2C_PORT = (i2c_port_t)BOARD_BME690_I2C_PORT;
static const TickType_t I2C_TIMEOUT = pdMS_TO_TICKS(100);

static struct bme69x_dev s_dev;
static struct bme69x_conf s_conf;
static SemaphoreHandle_t s_bus_lock;
static SemaphoreHandle_t s_api_lock;

static bool s_bus_installed;
static bool s_ready;
static uint8_t s_chip_id;
static uint8_t s_variant_id;
static int8_t s_last_status = BME69X_E_DEV_NOT_FOUND;

static void bme690_store_status(int8_t status)
{
    s_last_status = status;
    if (status == BME69X_E_COM_FAIL || status == BME69X_E_DEV_NOT_FOUND) {
        s_ready = false;
    }
}

static void bme690_delay_us(uint32_t period, void *intf_ptr)
{
    (void)intf_ptr;
    if (period >= 1000U) {
        uint32_t ticks = (period + 999U) / 1000U;
        vTaskDelay(pdMS_TO_TICKS(ticks));
    } else {
        esp_rom_delay_us(period);
    }
}

static int8_t bme690_i2c_read(uint8_t reg_addr, uint8_t *reg_data,
                              uint32_t length, void *intf_ptr)
{
    (void)intf_ptr;
    if (reg_data == NULL || length == 0 || s_bus_lock == NULL) {
        return -1;
    }
    if (xSemaphoreTake(s_bus_lock, I2C_TIMEOUT) != pdTRUE) {
        return -1;
    }

    esp_err_t err = i2c_master_write_read_device(I2C_PORT,
                                                 BOARD_BME690_I2C_ADDR,
                                                 &reg_addr, 1,
                                                 reg_data, length,
                                                 I2C_TIMEOUT);
    xSemaphoreGive(s_bus_lock);
    return err == ESP_OK ? 0 : -1;
}

static int8_t bme690_i2c_write(uint8_t reg_addr, const uint8_t *reg_data,
                               uint32_t length, void *intf_ptr)
{
    (void)intf_ptr;
    if (s_bus_lock == NULL) {
        return -1;
    }
    if (xSemaphoreTake(s_bus_lock, I2C_TIMEOUT) != pdTRUE) {
        return -1;
    }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (cmd == NULL) {
        xSemaphoreGive(s_bus_lock);
        return -1;
    }

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (BOARD_BME690_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);
    if (reg_data != NULL && length > 0) {
        i2c_master_write(cmd, reg_data, length, true);
    }
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(I2C_PORT, cmd, I2C_TIMEOUT);
    i2c_cmd_link_delete(cmd);

    xSemaphoreGive(s_bus_lock);
    return err == ESP_OK ? 0 : -1;
}

static esp_err_t bme690_bus_init(void)
{
    if (s_bus_installed) {
        return ESP_OK;
    }

    i2c_config_t cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = BOARD_BME690_I2C_SDA,
        .scl_io_num = BOARD_BME690_I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_DISABLE,
        .scl_pullup_en = GPIO_PULLUP_DISABLE,
        .master.clk_speed = BOARD_BME690_I2C_HZ,
        .clk_flags = 0,
    };

    esp_err_t err = i2c_param_config(I2C_PORT, &cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I²C 参数配置失败: %s", esp_err_to_name(err));
        return err;
    }

    err = i2c_driver_install(I2C_PORT, I2C_MODE_MASTER, 0, 0, 0);
    if (err == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "I²C%d 已安装，沿用现有总线", (int)I2C_PORT);
        err = ESP_OK;
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I²C 驱动安装失败: %s", esp_err_to_name(err));
        return err;
    }

    s_bus_installed = true;
    ESP_LOGI(TAG, "I²C 就绪: SDA=GPIO%d SCL=GPIO%d %d Hz addr=0x%02X",
             BOARD_BME690_I2C_SDA, BOARD_BME690_I2C_SCL,
             BOARD_BME690_I2C_HZ, BOARD_BME690_I2C_ADDR);
    return ESP_OK;
}

static esp_err_t bme690_locks_init(void)
{
    if (s_bus_lock == NULL) {
        s_bus_lock = xSemaphoreCreateMutex();
    }
    if (s_api_lock == NULL) {
        s_api_lock = xSemaphoreCreateMutex();
    }
    return (s_bus_lock != NULL && s_api_lock != NULL) ? ESP_OK : ESP_ERR_NO_MEM;
}

static esp_err_t bme690_probe_identity(void)
{
    uint8_t chip = 0;
    uint8_t variant = 0;

    if (bme690_i2c_read(BME69X_REG_CHIP_ID, &chip, 1, NULL) != 0) {
        ESP_LOGE(TAG, "0x%02X 无应答：查 VDD/VDDIO/GND、SDA=GPIO%d、SCL=GPIO%d、CSB 是否直连 VDDIO、SDO 是否接 GND",
                 BOARD_BME690_I2C_ADDR, BOARD_BME690_I2C_SDA, BOARD_BME690_I2C_SCL);
        return ESP_ERR_NOT_FOUND;
    }
    if (bme690_i2c_read(BME69X_REG_VARIANT_ID, &variant, 1, NULL) != 0) {
        ESP_LOGE(TAG, "读 Variant_Id(0xF0) 失败");
        return ESP_FAIL;
    }

    s_chip_id = chip;
    s_variant_id = variant;
    ESP_LOGI(TAG, "身份: Chip_Id=0x%02X Variant_Id=0x%02X (%s)",
             chip, variant, bme690_variant_name(variant));

    if (chip != BME69X_CHIP_ID) {
        ESP_LOGE(TAG, "Chip_Id 应为 0x61，实际 0x%02X", chip);
        return ESP_ERR_INVALID_RESPONSE;
    }
    if (variant != BME690_VARIANT_GAS_HIGH) {
        ESP_LOGW(TAG, "不是 BME690（Variant_Id 应为 0x02）；继续按兼容模式尝试");
    }
    return ESP_OK;
}

static esp_err_t bme690_init_locked(void)
{
    esp_err_t err = bme690_probe_identity();
    if (err != ESP_OK) {
        bme690_store_status(BME69X_E_DEV_NOT_FOUND);
        return err;
    }

    memset(&s_dev, 0, sizeof(s_dev));
    s_dev.intf = BME69X_I2C_INTF;
    s_dev.intf_ptr = NULL;
    s_dev.read = bme690_i2c_read;
    s_dev.write = bme690_i2c_write;
    s_dev.delay_us = bme690_delay_us;
    s_dev.amb_temp = 25;

    int8_t rslt = bme69x_init(&s_dev);
    bme690_store_status(rslt);
    if (rslt != BME69X_OK) {
        ESP_LOGE(TAG, "Bosch 初始化失败: %d (%s)", rslt, bme690_status_text(rslt));
        return ESP_FAIL;
    }

    s_chip_id = s_dev.chip_id;
    s_variant_id = (uint8_t)s_dev.variant_id;
    if (s_variant_id != BME690_VARIANT_GAS_HIGH) {
        ESP_LOGW(TAG, "驱动检测到 Variant_Id=0x%02X，非 0x02", s_variant_id);
    }

    s_conf.filter = BME69X_FILTER_OFF;
    s_conf.odr = BME69X_ODR_NONE;
    s_conf.os_hum = BME69X_OS_16X;
    s_conf.os_pres = BME69X_OS_16X;
    s_conf.os_temp = BME69X_OS_16X;
    rslt = bme69x_set_conf(&s_conf, &s_dev);
    bme690_store_status(rslt);
    if (rslt != BME69X_OK) {
        ESP_LOGE(TAG, "测量配置失败: %d (%s)", rslt, bme690_status_text(rslt));
        return ESP_FAIL;
    }

    s_ready = true;
    ESP_LOGI(TAG, "初始化完成: 16x T/P/H，forced 模式，校准参数已加载");
    return ESP_OK;
}

esp_err_t bme690_init(void)
{
    if (s_ready) {
        return ESP_OK;
    }

    esp_err_t err = bme690_locks_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "创建互斥锁失败");
        return err;
    }

    err = bme690_bus_init();
    if (err != ESP_OK) {
        return err;
    }

    if (xSemaphoreTake(s_api_lock, I2C_TIMEOUT) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    err = bme690_init_locked();
    xSemaphoreGive(s_api_lock);
    return err;
}

bool bme690_ready(void)
{
    return s_ready;
}

static esp_err_t bme690_measure_locked(bool enable_heater,
                                       uint16_t heater_temp_c,
                                       uint16_t heater_duration_ms,
                                       bme690_sample_t *out)
{
    /* 禁用加热时仍写入合法参数，避免 0℃ 进入电阻换算。 */
    if (!enable_heater) {
        heater_temp_c = BME690_DEFAULT_HEATER_TEMP_C;
        heater_duration_ms = BME690_DEFAULT_HEATER_DURATION_MS;
    }

    struct bme69x_heatr_conf heatr = {
        .enable = enable_heater ? BME69X_ENABLE : BME69X_DISABLE,
        .heatr_temp = heater_temp_c,
        .heatr_dur = heater_duration_ms,
        .heatr_temp_prof = NULL,
        .heatr_dur_prof = NULL,
        .profile_len = 0,
        .shared_heatr_dur = 0,
    };

    int8_t rslt = bme69x_set_heatr_conf(BME69X_FORCED_MODE, &heatr, &s_dev);
    bme690_store_status(rslt);
    if (rslt != BME69X_OK) {
        return ESP_FAIL;
    }

    rslt = bme69x_set_op_mode(BME69X_FORCED_MODE, &s_dev);
    bme690_store_status(rslt);
    if (rslt != BME69X_OK) {
        return ESP_FAIL;
    }

    uint32_t wait_us = bme69x_get_meas_dur(BME69X_FORCED_MODE, &s_conf, &s_dev);
    if (enable_heater) {
        wait_us += (uint32_t)heater_duration_ms * 1000U;
    }
    wait_us += 2000U;
    bme690_delay_us(wait_us, NULL);

    struct bme69x_data data;
    uint8_t n_fields = 0;
    memset(&data, 0, sizeof(data));
    rslt = bme69x_get_data(BME69X_FORCED_MODE, &data, &n_fields, &s_dev);
    bme690_store_status(rslt);
    if (rslt != BME69X_OK || n_fields == 0) {
        return ESP_ERR_INVALID_STATE;
    }

    memset(out, 0, sizeof(*out));
    out->valid = true;
    out->chip_id = s_chip_id;
    out->variant_id = s_variant_id;
    out->bosch_status = rslt;
#ifdef BME69X_USE_FPU
    /* 浮点 API：温度已是 °C，湿度已是 %RH；只有气压是 Pa。 */
    out->temperature_c = data.temperature;
    out->humidity_pct = data.humidity;
#else
    /* 定点 API：温度 x100，湿度 x1000。 */
    out->temperature_c = (float)data.temperature / 100.0f;
    out->humidity_pct = (float)data.humidity / 1000.0f;
#endif
    out->pressure_hpa = (float)data.pressure / 100.0f;
    out->gas_valid = enable_heater && ((data.status & BME69X_GASM_VALID_MSK) != 0);
    out->heat_stable = enable_heater && ((data.status & BME69X_HEAT_STAB_MSK) != 0);
    out->gas_resistance_ohm = out->gas_valid ? (float)data.gas_resistance : NAN;
    out->measurement_us = wait_us;
    out->timestamp_us = esp_timer_get_time();
    return ESP_OK;
}

static esp_err_t bme690_measure(bool enable_heater,
                                uint16_t heater_temp_c,
                                uint16_t heater_duration_ms,
                                bme690_sample_t *out)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_ready || s_api_lock == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xSemaphoreTake(s_api_lock, pdMS_TO_TICKS(2000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t err = bme690_measure_locked(enable_heater, heater_temp_c,
                                          heater_duration_ms, out);
    xSemaphoreGive(s_api_lock);
    return err;
}

esp_err_t bme690_read_environment(bme690_sample_t *out)
{
    return bme690_measure(false, 0, 0, out);
}

esp_err_t bme690_read_gas(bme690_sample_t *out,
                          uint16_t heater_temp_c,
                          uint16_t heater_duration_ms)
{
    if (heater_temp_c < 200 || heater_temp_c > 400 ||
        heater_duration_ms == 0 || heater_duration_ms > 1000) {
        return ESP_ERR_INVALID_ARG;
    }
    return bme690_measure(true, heater_temp_c, heater_duration_ms, out);
}

uint8_t bme690_chip_id(void)
{
    return s_chip_id;
}

uint8_t bme690_variant_id(void)
{
    return s_variant_id;
}

int8_t bme690_last_status(void)
{
    return s_last_status;
}

const char *bme690_variant_name(uint8_t variant_id)
{
    switch (variant_id) {
    case BME69X_VARIANT_GAS_LOW:  return "BME680/688 low-gas variant";
    case BME69X_VARIANT_GAS_HIGH: return "BME688 high-gas variant";
    case BME690_VARIANT_GAS_HIGH: return "BME690";
    default:                      return "unknown variant";
    }
}

const char *bme690_status_text(int8_t status)
{
    switch (status) {
    case BME69X_OK:                  return "ok";
    case BME69X_E_NULL_PTR:          return "null pointer";
    case BME69X_E_COM_FAIL:          return "I2C communication failure";
    case BME69X_E_DEV_NOT_FOUND:     return "device not found";
    case BME69X_E_INVALID_LENGTH:    return "invalid length";
    case BME69X_E_SELF_TEST:         return "self-test failed";
    case BME69X_W_DEFINE_OP_MODE:    return "undefined op mode";
    case BME69X_W_NO_NEW_DATA:       return "no new data";
    case BME69X_W_DEFINE_SHD_HEATR_DUR: return "undefined shared heater duration";
    default:                         return "unknown status";
    }
}
