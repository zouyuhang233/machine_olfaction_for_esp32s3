/*
 * services.h -- 业务服务层（BME690 为真实采集，其余外设仍为模拟实现）
 *
 * UI 只调用这里，不直接碰 Wi-Fi/BLE/音频/传感器/Modbus。
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HISTORY_MAX_RECORDS 20

typedef struct {
    uint32_t sequence;
    float temp_c;
    float humi_pct;
    float pressure_hpa;
    float rs_kohm;
    bool valid;
    bool gas_valid;
} history_record_t;

typedef struct {
    /* BME690 没有 TVOC/eCO2 寄存器；未接 BSEC 时这两个值恒为 NAN。 */
    float tvoc;    /* ppb, NAN = 不可用 */
    float eco2;    /* ppm, NAN = 不可用 */
    float temp;    /* °C, NAN = 不可用 */
    float humi;    /* % */
    float press;   /* hPa */
    float rs;      /* kΩ */

    bool present;         /* I²C 上找到 BME690 */
    bool valid;           /* 最近一次环境测量成功 */
    bool gas_valid;       /* 最近一次 Rs 有效 */
    uint8_t chip_id;      /* 0x61 */
    uint8_t variant_id;   /* BME690 = 0x02 */
    uint32_t sample_count;
    uint32_t age_ms;      /* 距最近一次成功采样 */
} sensor_data_t;

void services_init(void);
void services_update(void);

void sensor_get(sensor_data_t *out);
const char *sensor_status_text(void);

bool history_capture_current(void);
int history_count(void);
bool history_get(int newest_index, history_record_t *out);

bool net_wifi_enabled(void);
void net_wifi_set_enabled(bool en);
bool net_wifi_connected(void);
const char *net_wifi_status_text(void);
const char *net_wifi_ssid(void);
const char *net_ip(void);
uint8_t net_wifi_signal(void);

bool net_ble_enabled(void);
void net_ble_set_enabled(bool en);
bool net_ble_connected(void);
const char *net_ble_status_text(void);
const char *net_ble_name(void);

bool audio_prompt_enabled(void);
void audio_set_prompt_enabled(bool en);
uint8_t audio_volume(void);
void audio_set_volume(uint8_t v);
void audio_play_test(void);

uint8_t modbus_addr(void);
void modbus_set_addr(uint8_t addr);
uint32_t modbus_baud(void);
void modbus_set_baud(uint32_t baud);
const char *modbus_status_text(void);

void services_device_id(char *buf, size_t len);

#ifdef __cplusplus
}
#endif
