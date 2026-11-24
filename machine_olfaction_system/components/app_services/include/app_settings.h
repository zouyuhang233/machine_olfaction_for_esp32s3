/*
 * app_settings.h -- 可持久化的应用设置
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool wifi_enabled;
    bool ble_enabled;
    uint8_t volume;         /* 0..100 */
    bool voice_prompt;
    uint8_t brightness;     /* 10..100，背光 PWM */
    uint8_t rotation;       /* 0..3 */
    uint8_t modbus_addr;    /* 1..247 */
    uint32_t modbus_baud;   /* 9600..115200 */
    char server[64];        /* MQTT / HTTP 服务器地址 */
} app_settings_t;

#ifdef __cplusplus
}
#endif
