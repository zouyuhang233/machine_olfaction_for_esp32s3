/*
 * app_main.c -- 显示子系统 + BME690 采集自检
 *
 * 上电流程：
 *   1. 初始化屏幕，显示启动信息；
 *   2. 初始化 BME690（I²C0，SDA=GPIO8，SCL=GPIO18，0x76）；
 *   3. 每秒读一次温湿度/气压/气体电阻，串口打印；每 2 秒刷一次屏；
 *   4. 传感器没接/初始化失败时不死机：屏幕提示 ERR，后台每 2 秒重试。
 *
 * 调试入口：串口 115200 8N1，看 "bme690" 与 "app" 两个 TAG 的日志。
 */
#include "bme690.h"
#include "board_pins.h"
#include "bsp.h"
#include "esp_log.h"
#include "font.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lcd.h"
#include "stdio.h"

static const char *TAG = "app";

#define LCD_ROTATION 3   /* 3 = 横屏 160x128，在方向 1 的基础上再转 180° */

static void draw_header(const char *status, uint16_t color)
{
    lcd_clear(LCD_BLACK);
    lcd_draw_text(&font8x12, 0, 0, status, color, LCD_BLACK);
    lcd_draw_text(&font8x12, 0, 14, "SDA8 SCL18 0x76", LCD_WHITE, LCD_BLACK);
}

static void draw_reading(const bme690_reading_t *r)
{
    char line[32];

    lcd_clear(LCD_BLACK);
    lcd_draw_text(&font8x12, 0, 0, "BME690 OK", LCD_GREEN, LCD_BLACK);

    snprintf(line, sizeof(line), "T %6.2f C", r->temperature_c);
    lcd_draw_text(&font8x12, 0, 16, line, LCD_WHITE, LCD_BLACK);

    snprintf(line, sizeof(line), "H %6.2f %%", r->humidity_pct);
    lcd_draw_text(&font8x12, 0, 30, line, LCD_WHITE, LCD_BLACK);

    snprintf(line, sizeof(line), "P %7.1f hPa", r->pressure_pa / 100.0f);
    lcd_draw_text(&font8x12, 0, 44, line, LCD_WHITE, LCD_BLACK);

    snprintf(line, sizeof(line), "Rs %7.1f k", r->gas_resistance_ohm / 1000.0f);
    lcd_draw_text(&font8x12, 0, 58, line, LCD_YELLOW, LCD_BLACK);

    snprintf(line, sizeof(line), "st 0x%02X idx %u", r->status, r->gas_index);
    lcd_draw_text(&font8x12, 0, 72, line, LCD_CYAN, LCD_BLACK);
}

void app_main(void)
{
    ESP_ERROR_CHECK(bsp_init());
    ESP_ERROR_CHECK(lcd_init());
    ESP_ERROR_CHECK(lcd_set_rotation(LCD_ROTATION));

    draw_header("BME690 init...", LCD_YELLOW);
    ESP_ERROR_CHECK(lcd_flush());
    bsp_backlight(true);

    esp_err_t err = bme690_init(BOARD_BME690_I2C_PORT, BOARD_BME690_SDA,
                                BOARD_BME690_SCL, BOARD_BME690_I2C_ADDR);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "BME690 OK: chip_id=0x%02X variant_id=0x%02X",
                 bme690_chip_id(), bme690_variant_id());
        (void)bme690_log_registers();
    } else {
        ESP_LOGE(TAG, "BME690 初始化失败: %s，屏幕继续显示，后台每 2s 重试",
                 esp_err_to_name(err));
    }

    uint32_t tick = 0;
    bool shown_error = false;
    for (;;) {
        if (!bme690_ready()) {
            if (!shown_error) {
                draw_header("BME690 ERR", LCD_RED);
                lcd_draw_text(&font8x12, 0, 28, "check CSB/pullup", LCD_WHITE, LCD_BLACK);
                lcd_draw_text(&font8x12, 0, 42, "SDO->GND=0x76", LCD_WHITE, LCD_BLACK);
                lcd_draw_text(&font8x12, 0, 56, "retry 2s...", LCD_YELLOW, LCD_BLACK);
                (void)lcd_flush();
                shown_error = true;
            }
            vTaskDelay(pdMS_TO_TICKS(2000));
            esp_err_t retry = bme690_init(BOARD_BME690_I2C_PORT, BOARD_BME690_SDA,
                                          BOARD_BME690_SCL, BOARD_BME690_I2C_ADDR);
            if (retry == ESP_OK) {
                shown_error = false;
            }
            continue;
        }

        bme690_reading_t r;
        err = bme690_read(&r);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "T=%.2fC H=%.2f%% P=%.1fPa Rs=%.0fOhm status=0x%02X gas_idx=%u",
                     r.temperature_c, r.humidity_pct, r.pressure_pa,
                     r.gas_resistance_ohm, r.status, r.gas_index);

            if ((tick % 2U) == 0U) {
                draw_reading(&r);
                (void)lcd_flush();
            }
        } else {
            ESP_LOGW(TAG, "BME690 读取失败: %s", esp_err_to_name(err));
        }

        tick++;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
