/*
 * settings_store.h -- NVS 设置读写
 */
#pragma once

#include "app_settings.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t settings_store_init(void);
app_settings_t *settings_get(void);
esp_err_t settings_save(void);
void settings_reset_defaults(void);

#ifdef __cplusplus
}
#endif
