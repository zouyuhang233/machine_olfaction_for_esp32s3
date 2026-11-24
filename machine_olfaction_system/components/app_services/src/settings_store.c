#include "settings_store.h"

#include <string.h>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "settings";
static const char *NVS_NS = "appcfg";

static nvs_handle_t s_nvs;
static app_settings_t s_settings;

static void set_defaults(void)
{
    memset(&s_settings, 0, sizeof(s_settings));
    s_settings.wifi_enabled = false;
    s_settings.ble_enabled = false;
    s_settings.volume = 70;
    s_settings.voice_prompt = true;
    s_settings.brightness = 100;
    s_settings.rotation = 0;
    s_settings.modbus_addr = 1;
    s_settings.modbus_baud = 115200;
    strncpy(s_settings.server, "mqtt://192.168.1.100", sizeof(s_settings.server) - 1);
}

esp_err_t settings_store_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_flash_init 失败: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_open(NVS_NS, NVS_READWRITE, &s_nvs);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open 失败: %s", esp_err_to_name(err));
        return err;
    }

    set_defaults();

    uint8_t u8;
    uint32_t u32;
    size_t len;
    uint8_t display_cfg_ver = 0;
    bool migrate_rotation = nvs_get_u8(s_nvs, "disp_ver", &display_cfg_ver) != ESP_OK || display_cfg_ver < 2;

    if (nvs_get_u8(s_nvs, "wifi_en", &u8) == ESP_OK) s_settings.wifi_enabled = u8;
    if (nvs_get_u8(s_nvs, "ble_en", &u8) == ESP_OK) s_settings.ble_enabled = u8;
    if (nvs_get_u8(s_nvs, "volume", &u8) == ESP_OK) s_settings.volume = u8;
    if (nvs_get_u8(s_nvs, "prompt", &u8) == ESP_OK) s_settings.voice_prompt = u8;
    if (nvs_get_u8(s_nvs, "bright", &u8) == ESP_OK) s_settings.brightness = u8;
    if (nvs_get_u8(s_nvs, "rotation", &u8) == ESP_OK) s_settings.rotation = u8 & 3;
    if (nvs_get_u8(s_nvs, "mb_addr", &u8) == ESP_OK) s_settings.modbus_addr = u8;
    if (nvs_get_u32(s_nvs, "mb_baud", &u32) == ESP_OK) s_settings.modbus_baud = u32;
    len = sizeof(s_settings.server);
    nvs_get_str(s_nvs, "server", s_settings.server, &len);

    /* 配置做了跨版本迁移时也保证 UI 能安全使用。 */
    if (s_settings.volume > 100) s_settings.volume = 100;
    if (s_settings.brightness < 10) s_settings.brightness = 10;
    if (s_settings.brightness > 100) s_settings.brightness = 100;
    s_settings.rotation &= 3;
    if (migrate_rotation) s_settings.rotation = 0;
    if (s_settings.modbus_addr < 1) s_settings.modbus_addr = 1;
    if (s_settings.modbus_addr > 247) s_settings.modbus_addr = 247;

    if (migrate_rotation) {
        settings_save();
    }
    ESP_LOGI(TAG, "设置已加载: wifi=%d ble=%d vol=%d rot=%d",
             s_settings.wifi_enabled, s_settings.ble_enabled,
             s_settings.volume, s_settings.rotation);
    return ESP_OK;
}

app_settings_t *settings_get(void)
{
    return &s_settings;
}

esp_err_t settings_save(void)
{
    esp_err_t err = ESP_OK;
    err |= nvs_set_u8(s_nvs, "wifi_en", s_settings.wifi_enabled);
    err |= nvs_set_u8(s_nvs, "ble_en", s_settings.ble_enabled);
    err |= nvs_set_u8(s_nvs, "volume", s_settings.volume);
    err |= nvs_set_u8(s_nvs, "prompt", s_settings.voice_prompt);
    err |= nvs_set_u8(s_nvs, "bright", s_settings.brightness);
    err |= nvs_set_u8(s_nvs, "rotation", s_settings.rotation);
    err |= nvs_set_u8(s_nvs, "disp_ver", 2);
    err |= nvs_set_u8(s_nvs, "mb_addr", s_settings.modbus_addr);
    err |= nvs_set_u32(s_nvs, "mb_baud", s_settings.modbus_baud);
    err |= nvs_set_str(s_nvs, "server", s_settings.server);
    err |= nvs_commit(s_nvs);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "保存设置失败: %s", esp_err_to_name(err));
    }
    return err;
}

void settings_reset_defaults(void)
{
    set_defaults();
    settings_save();
}



