#include "services.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "app_settings.h"
#include "bme690.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "settings_store.h"

static const char *TAG = "services";

#define SENSOR_RETRY_MS      10000
#define SENSOR_TASK_STACK    6144
#define SENSOR_TASK_PRIO     5

typedef struct {
    sensor_data_t data;
    int64_t sample_us;
} sensor_cache_t;

static sensor_cache_t s_sensor_cache;
static SemaphoreHandle_t s_sensor_lock;

static void sensor_cache_reset(void)
{
    memset(&s_sensor_cache, 0, sizeof(s_sensor_cache));
    s_sensor_cache.data.tvoc = NAN;
    s_sensor_cache.data.eco2 = NAN;
    s_sensor_cache.data.temp = NAN;
    s_sensor_cache.data.humi = NAN;
    s_sensor_cache.data.press = NAN;
    s_sensor_cache.data.rs = NAN;
    s_sensor_cache.data.present = false;
    s_sensor_cache.data.valid = false;
    s_sensor_cache.data.gas_valid = false;
    s_sensor_cache.data.age_ms = UINT32_MAX;
}

static void sensor_cache_store(const bme690_sample_t *sample)
{
    if (sample == NULL || s_sensor_lock == NULL) {
        return;
    }

    if (xSemaphoreTake(s_sensor_lock, pdMS_TO_TICKS(100)) != pdTRUE) {
        return;
    }

    sensor_data_t *d = &s_sensor_cache.data;
    d->tvoc = NAN;
    d->eco2 = NAN;
    d->temp = sample->valid ? sample->temperature_c : NAN;
    d->humi = sample->valid ? sample->humidity_pct : NAN;
    d->press = sample->valid ? sample->pressure_hpa : NAN;
    d->rs = (sample->valid && sample->gas_valid &&
             isfinite(sample->gas_resistance_ohm))
                ? sample->gas_resistance_ohm / 1000.0f
                : NAN;
    d->present = true;
    d->valid = sample->valid;
    d->gas_valid = sample->gas_valid;
    d->chip_id = sample->chip_id;
    d->variant_id = sample->variant_id;
    d->age_ms = 0;
    d->sample_count++;
    s_sensor_cache.sample_us = sample->timestamp_us;

    xSemaphoreGive(s_sensor_lock);
}

static void sensor_cache_mark_failed(bool present)
{
    if (s_sensor_lock == NULL) {
        return;
    }
    if (xSemaphoreTake(s_sensor_lock, pdMS_TO_TICKS(100)) != pdTRUE) {
        return;
    }

    sensor_data_t *d = &s_sensor_cache.data;
    d->present = present;
    d->valid = false;
    d->gas_valid = false;
    d->temp = NAN;
    d->humi = NAN;
    d->press = NAN;
    d->rs = NAN;
    d->chip_id = bme690_chip_id();
    d->variant_id = bme690_variant_id();
    d->age_ms = s_sensor_cache.sample_us == 0
                    ? UINT32_MAX
                    : (uint32_t)((esp_timer_get_time() - s_sensor_cache.sample_us) / 1000);

    xSemaphoreGive(s_sensor_lock);
}

static void sensor_task(void *arg)
{
    (void)arg;
    bool env_logged = false;
    uint32_t good_samples = 0;

    for (;;) {
        esp_err_t err = bme690_init();
        if (err != ESP_OK) {
            sensor_cache_mark_failed(false);
            ESP_LOGW(TAG, "BME690 初始化失败: %s，%d ms 后重试",
                     esp_err_to_name(err), SENSOR_RETRY_MS);
            vTaskDelay(pdMS_TO_TICKS(SENSOR_RETRY_MS));
            continue;
        }

        if (!env_logged) {
            bme690_sample_t env;
            err = bme690_read_environment(&env);
            if (err == ESP_OK) {
                sensor_cache_store(&env);
                ESP_LOGI(TAG, "BME690 环境自检: %.2f°C, %.2f%%RH, %.2f hPa",
                         env.temperature_c, env.humidity_pct, env.pressure_hpa);
                env_logged = true;
            } else {
                ESP_LOGW(TAG, "BME690 环境自检失败: %s (%d/%s)",
                         esp_err_to_name(err), bme690_last_status(),
                         bme690_status_text(bme690_last_status()));
            }
        }

        bme690_sample_t sample;
        memset(&sample, 0, sizeof(sample));
        err = bme690_read_gas(&sample,
                              BME690_DEFAULT_HEATER_TEMP_C,
                              BME690_DEFAULT_HEATER_DURATION_MS);
        if (err == ESP_OK) {
            sensor_cache_store(&sample);
            good_samples++;
            if (good_samples == 1 || (good_samples % 10U) == 0U) {
                ESP_LOGI(TAG, "BME690 #%lu: %.2f°C %.2f%% %.2f hPa Rs=%.1f kΩ %s",
                         (unsigned long)good_samples,
                         sample.temperature_c, sample.humidity_pct,
                         sample.pressure_hpa,
                         isfinite(sample.gas_resistance_ohm)
                             ? sample.gas_resistance_ohm / 1000.0f : NAN,
                         sample.gas_valid
                             ? (sample.heat_stable ? "gas-valid/heat-stable" : "gas-valid/heat-warming")
                             : "gas-invalid");
            }
        } else {
            sensor_cache_mark_failed(true);
            ESP_LOGW(TAG, "BME690 采集失败: %s (%d/%s)",
                     esp_err_to_name(err), bme690_last_status(),
                     bme690_status_text(bme690_last_status()));
        }

        vTaskDelay(pdMS_TO_TICKS(BME690_SAMPLE_PERIOD_MS));
    }
}

void services_init(void)
{
    sensor_cache_reset();
    s_sensor_lock = xSemaphoreCreateMutex();

    if (s_sensor_lock == NULL) {
        ESP_LOGE(TAG, "传感器缓存互斥锁创建失败");
    } else if (xTaskCreate(sensor_task, "bme690_sensor", SENSOR_TASK_STACK,
                           NULL, SENSOR_TASK_PRIO, NULL) != pdPASS) {
        ESP_LOGE(TAG, "BME690 采集任务创建失败");
    } else {
        ESP_LOGI(TAG, "BME690 采集任务已启动（每 %d ms 一次 forced 测量）",
                 BME690_SAMPLE_PERIOD_MS);
    }

    ESP_LOGI(TAG, "服务层初始化（Wi-Fi/BLE/音频/Modbus 仍为模拟实现）");
}

void services_update(void)
{
}

void sensor_get(sensor_data_t *out)
{
    if (out == NULL) {
        return;
    }

    memset(out, 0, sizeof(*out));
    out->tvoc = NAN;
    out->eco2 = NAN;
    out->temp = NAN;
    out->humi = NAN;
    out->press = NAN;
    out->rs = NAN;
    out->age_ms = UINT32_MAX;

    if (s_sensor_lock == NULL) {
        return;
    }
    if (xSemaphoreTake(s_sensor_lock, pdMS_TO_TICKS(100)) != pdTRUE) {
        out->present = bme690_ready();
        return;
    }

    *out = s_sensor_cache.data;
    if (s_sensor_cache.sample_us != 0) {
        out->age_ms = (uint32_t)((esp_timer_get_time() - s_sensor_cache.sample_us) / 1000);
    }
    xSemaphoreGive(s_sensor_lock);
}

const char *sensor_status_text(void)
{
    sensor_data_t d;
    sensor_get(&d);
    if (!d.present) {
        return "BME690 未找到";
    }
    if (!d.valid) {
        return "BME690 读取失败";
    }
    return d.gas_valid ? "BME690 正常" : "BME690 温湿度正常/Rs无效";
}

bool net_wifi_enabled(void)
{
    return settings_get()->wifi_enabled;
}

void net_wifi_set_enabled(bool en)
{
    app_settings_t *s = settings_get();
    if (s->wifi_enabled == en) {
        return;
    }
    s->wifi_enabled = en;
    settings_save();
}

bool net_wifi_connected(void)
{
    /* 真实 Wi-Fi 协议接入前不得伪造成已连接。 */
    return false;
}

const char *net_wifi_status_text(void)
{
    const app_settings_t *s = settings_get();
    if (!s->wifi_enabled) {
        return "已关闭";
    }
    if (!net_wifi_connected()) {
        return "连接中";
    }
    return "已连接";
}

const char *net_wifi_ssid(void)
{
    return "HomeWiFi";
}

const char *net_ip(void)
{
    return net_wifi_connected() ? "192.168.1.42" : "0.0.0.0";
}

uint8_t net_wifi_signal(void)
{
    return net_wifi_connected() ? 80 : 0;
}

bool net_ble_enabled(void)
{
    return settings_get()->ble_enabled;
}

void net_ble_set_enabled(bool en)
{
    app_settings_t *s = settings_get();
    if (s->ble_enabled == en) {
        return;
    }
    s->ble_enabled = en;
    settings_save();
}

bool net_ble_connected(void)
{
    /* 真实 BLE 连接事件接入前不得伪造成已连接。 */
    return false;
}

const char *net_ble_status_text(void)
{
    const app_settings_t *s = settings_get();
    if (!s->ble_enabled) {
        return "已关闭";
    }
    return net_ble_connected() ? "已连接" : "广播中";
}

const char *net_ble_name(void)
{
    return "Olfaction-01";
}

bool audio_prompt_enabled(void)
{
    return settings_get()->voice_prompt;
}

void audio_set_prompt_enabled(bool en)
{
    settings_get()->voice_prompt = en;
    settings_save();
}

uint8_t audio_volume(void)
{
    return settings_get()->volume;
}

void audio_set_volume(uint8_t v)
{
    if (v > 100) {
        v = 100;
    }
    settings_get()->volume = v;
    settings_save();
}

void audio_play_test(void)
{
    ESP_LOGI(TAG, "测试播放（LU-03T 串口待接入），音量 %d，提示音 %s",
             audio_volume(), audio_prompt_enabled() ? "开" : "关");
}

uint8_t modbus_addr(void)
{
    return settings_get()->modbus_addr;
}

void modbus_set_addr(uint8_t addr)
{
    if (addr < 1) {
        addr = 1;
    }
    if (addr > 247) {
        addr = 247;
    }
    settings_get()->modbus_addr = addr;
    settings_save();
}

uint32_t modbus_baud(void)
{
    return settings_get()->modbus_baud;
}

void modbus_set_baud(uint32_t baud)
{
    settings_get()->modbus_baud = baud;
    settings_save();
}

const char *modbus_status_text(void)
{
    return "从站等待";
}

void services_device_id(char *buf, size_t len)
{
    if (buf == NULL || len == 0) {
        return;
    }
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(buf, len, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}
