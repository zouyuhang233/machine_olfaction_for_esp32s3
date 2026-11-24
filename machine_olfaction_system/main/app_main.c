/*
 * app_main.c -- 应用入口
 *
 * 初始化顺序：板级 → 设置(NVS) → 屏 → LVGL 移植 → 服务层 → UI → 启动 LVGL 任务
 */
#include "bsp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lcd.h"
#include "lvgl_port.h"
#include "services.h"
#include "settings_store.h"
#include "app_ui.h"

static const char *TAG = "app";

void app_main(void)
{
    ESP_ERROR_CHECK(bsp_init());
    ESP_ERROR_CHECK(settings_store_init());

    ESP_ERROR_CHECK(lcd_init());
    ESP_ERROR_CHECK(lcd_set_rotation(settings_get()->rotation & 3));

    ESP_ERROR_CHECK(lvgl_port_init());
    services_init();
    ESP_ERROR_CHECK(app_ui_init());
    ESP_ERROR_CHECK(lvgl_port_start());

    bsp_backlight_set(settings_get()->brightness);
    ESP_LOGI(TAG, "应用 UI 已启动");

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
