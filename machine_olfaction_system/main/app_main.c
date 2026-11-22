/*
 * app_main.c -- 横屏 160x128，从左上角 (0,0) 显示 "zyh"（8x16 字库）
 *
 * 分层：
 *   bsp    引脚 / SPI 总线 / 背光 / 复位      （components/bsp）
 *   lcd    ST7735S 初始化 + 旋转 + 帧缓冲 + 画点/画字（components/lcd）
 *   font   8x16 / 8x12 两档 ASCII 点阵字库    （components/font）
 *   main   只调用上面三层
 */
#include "bsp.h"
#include "esp_log.h"
#include "font.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lcd.h"

static const char *TAG = "app";

#define LCD_ROTATION 3   /* 3 = 横屏 160x128，在方向 1 的基础上再转 180° */

void app_main(void)
{
    ESP_ERROR_CHECK(bsp_init());
    ESP_ERROR_CHECK(lcd_init());
    ESP_ERROR_CHECK(lcd_set_rotation(LCD_ROTATION));

    lcd_clear(LCD_BLACK);

    /* 8x16 字库，从左上角 (0,0) 开始显示 */
    lcd_draw_text(&font8x16, 0, 0, "zyh", LCD_WHITE, LCD_BLACK);

    ESP_ERROR_CHECK(lcd_flush());
    bsp_backlight(true);

    ESP_LOGI(TAG, "横屏 %dx%d，zyh 从 (0,0) 开始显示（8x16）", lcd_width(), lcd_height());

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
