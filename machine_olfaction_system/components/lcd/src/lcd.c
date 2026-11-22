/*
 * lcd.c -- ST7735S 驱动实现（18 位 RGB666 输出，支持 4 个方向）
 */
#include "lcd.h"

#include <stdlib.h>
#include <string.h>

#include "bsp.h"
#include "board_pins.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "lcd";

/* MADCTL：0=竖屏 0xC0，1=横屏 0xA0，2=倒置 0x00，3=横屏 0x60（厂商例程给出的四档） */
static const uint8_t s_madctl[4] = {0xC0, 0xA0, 0x00, 0x60};

#define FB_MAX_W 160
#define FB_MAX_H 160

static spi_device_handle_t s_dev;
static uint16_t *s_fb;                  /* 帧缓冲，20480 像素 = 40 KB */
static uint8_t s_row[FB_MAX_W * 3];     /* 18 位行缓冲：160*3 = 480 B */
static int s_w = BOARD_LCD_WIDTH;
static int s_h = BOARD_LCD_HEIGHT;
static int s_rot;
static bool s_inited;

/* ------------------------------ 底层收发 ------------------------------ */
static void lcd_cmd(uint8_t cmd)
{
    spi_transaction_t t = {.length = 8, .tx_buffer = &cmd};
    bsp_lcd_dc(false);
    /* 必须检查返回值：传输被驱动拒绝时不能静默失败（否则就是花屏） */
    ESP_ERROR_CHECK(spi_device_polling_transmit(s_dev, &t));
}

static void lcd_data(const uint8_t *buf, size_t len)
{
    if (len == 0) {
        return;
    }
    spi_transaction_t t = {.length = len * 8, .tx_buffer = buf};
    bsp_lcd_dc(true);
    ESP_ERROR_CHECK(spi_device_polling_transmit(s_dev, &t));
}

static void lcd_cmd_data(uint8_t cmd, const uint8_t *data, size_t len)
{
    lcd_cmd(cmd);
    lcd_data(data, len);
}

static void lcd_set_window(int x0, int y0, int x1, int y1)
{
    uint8_t caset[4] = {(uint8_t)(x0 >> 8), (uint8_t)x0, (uint8_t)(x1 >> 8), (uint8_t)x1};
    uint8_t raset[4] = {(uint8_t)(y0 >> 8), (uint8_t)y0, (uint8_t)(y1 >> 8), (uint8_t)y1};
    lcd_cmd_data(0x2A, caset, 4);
    lcd_cmd_data(0x2B, raset, 4);
}

/* ------------------------------ 初始化 ------------------------------ */
static void lcd_init_sequence(void)
{
    static const uint8_t madctl0[] = {0x00};
    static const uint8_t colmod[] = {0x06};                 /* 18bit RGB666 */
    static const uint8_t b1[] = {0x05, 0x3C, 0x3C};
    static const uint8_t b2[] = {0x05, 0x3C, 0x3C};
    static const uint8_t b3[] = {0x05, 0x3C, 0x3C, 0x05, 0x3C, 0x3C};
    static const uint8_t b4[] = {0x03};
    static const uint8_t c0[] = {0x2E, 0x06, 0x04};
    static const uint8_t c1[] = {0xC0, 0xC2};
    static const uint8_t c2[] = {0x0D, 0x0D};
    static const uint8_t c3[] = {0x8D, 0xEE};
    static const uint8_t c4[] = {0x8D, 0xEE};
    static const uint8_t c5[] = {0x00};
    static const uint8_t gm_p[] = {0x1B, 0x21, 0x10, 0x15, 0x2B, 0x25, 0x1F, 0x23,
                                   0x22, 0x22, 0x2B, 0x37, 0x00, 0x15, 0x02, 0x3F};
    static const uint8_t gm_n[] = {0x1A, 0x20, 0x0F, 0x15, 0x2A, 0x25, 0x1E, 0x23,
                                   0x23, 0x22, 0x2B, 0x37, 0x00, 0x15, 0x02, 0x3F};

    lcd_cmd(0x11);                                          /* 唤醒 */
    vTaskDelay(pdMS_TO_TICKS(120));

    lcd_cmd_data(0x36, madctl0, sizeof(madctl0));
    lcd_cmd_data(0x3A, colmod, sizeof(colmod));
    lcd_cmd_data(0xB1, b1, sizeof(b1));
    lcd_cmd_data(0xB2, b2, sizeof(b2));
    lcd_cmd_data(0xB3, b3, sizeof(b3));
    lcd_cmd_data(0xB4, b4, sizeof(b4));
    lcd_cmd_data(0xC0, c0, sizeof(c0));
    lcd_cmd_data(0xC1, c1, sizeof(c1));
    lcd_cmd_data(0xC2, c2, sizeof(c2));
    lcd_cmd_data(0xC3, c3, sizeof(c3));
    lcd_cmd_data(0xC4, c4, sizeof(c4));
    lcd_cmd_data(0xC5, c5, sizeof(c5));
    lcd_cmd_data(0x36, &s_madctl[0], 1);
    lcd_cmd_data(0xE0, gm_p, sizeof(gm_p));
    lcd_cmd_data(0xE1, gm_n, sizeof(gm_n));
    lcd_cmd(0x2C);
    lcd_cmd(0x29);                                          /* 开显示 */
    vTaskDelay(pdMS_TO_TICKS(50));
}

esp_err_t lcd_init(void)
{
    if (s_inited) {
        return ESP_OK;
    }

    esp_err_t err = bsp_lcd_add_device(&s_dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "挂 SPI 设备失败: %s", esp_err_to_name(err));
        return err;
    }

    s_fb = (uint16_t *)malloc(BOARD_LCD_WIDTH * BOARD_LCD_HEIGHT * sizeof(uint16_t));
    if (s_fb == NULL) {
        ESP_LOGE(TAG, "帧缓冲分配失败");
        return ESP_ERR_NO_MEM;
    }
    memset(s_fb, 0, BOARD_LCD_WIDTH * BOARD_LCD_HEIGHT * sizeof(uint16_t));

    bsp_lcd_reset();
    lcd_init_sequence();
    s_rot = 0;
    s_w = BOARD_LCD_WIDTH;
    s_h = BOARD_LCD_HEIGHT;
    s_inited = true;

    ESP_LOGI(TAG, "ST7735S 就绪: 18bit RGB666, 窗口(0,0), 当前 %dx%d", s_w, s_h);
    return ESP_OK;
}

esp_err_t lcd_set_rotation(int rot)
{
    if (!s_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    rot &= 3;
    lcd_cmd_data(0x36, &s_madctl[rot], 1);
    s_rot = rot;
    s_w = (rot & 1) ? BOARD_LCD_HEIGHT : BOARD_LCD_WIDTH;
    s_h = (rot & 1) ? BOARD_LCD_WIDTH : BOARD_LCD_HEIGHT;
    ESP_LOGI(TAG, "旋转 %d: %dx%d, MADCTL=0x%02X", rot, s_w, s_h, s_madctl[rot]);
    return ESP_OK;
}

int lcd_width(void)
{
    return s_w;
}

int lcd_height(void)
{
    return s_h;
}

/* ------------------------------ 绘制（写帧缓冲） ------------------------------ */
void lcd_draw_pixel(int x, int y, uint16_t color)
{
    if (s_fb == NULL || x < 0 || x >= s_w || y < 0 || y >= s_h) {
        return;
    }
    s_fb[(size_t)y * s_w + x] = color;
}

void lcd_clear(uint16_t color)
{
    if (s_fb == NULL) {
        return;
    }
    for (size_t i = 0; i < (size_t)s_w * s_h; i++) {
        s_fb[i] = color;
    }
}

void lcd_fill_rect(int x, int y, int w, int h, uint16_t color)
{
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            lcd_draw_pixel(x + i, y + j, color);
        }
    }
}

void lcd_draw_char(const lcd_font_t *font, int x, int y, char c, uint16_t fg, uint16_t bg)
{
    if (font == NULL) {
        return;
    }
    const uint8_t *glyph = font_glyph(font, c);
    for (int row = 0; row < font->h; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < font->w; col++) {
            bool on = (bits >> (7 - col)) & 1;
            lcd_draw_pixel(x + col, y + row, on ? fg : bg);
        }
    }
}

int lcd_draw_text(const lcd_font_t *font, int x, int y, const char *s, uint16_t fg, uint16_t bg)
{
    if (font == NULL || s == NULL) {
        return 0;
    }
    int cx = x;
    for (const char *p = s; *p != '\0'; p++) {
        lcd_draw_char(font, cx, y, *p, fg, bg);
        cx += font->advance;
    }
    return cx - x;
}

void lcd_draw_text_center(const lcd_font_t *font, int cx, int y, const char *s, uint16_t fg, uint16_t bg)
{
    if (font == NULL || s == NULL) {
        return;
    }
    int w = (int)strlen(s) * font->advance;
    lcd_draw_text(font, cx - w / 2, y, s, fg, bg);
}

/* ------------------------------ 刷屏 ------------------------------ */
esp_err_t lcd_flush(void)
{
    if (!s_inited || s_fb == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    for (int y = 0; y < s_h; y++) {
        const uint16_t *src = &s_fb[(size_t)y * s_w];
        for (int x = 0; x < s_w; x++) {
            uint16_t c = src[x];
            uint8_t r5 = (c >> 11) & 0x1F;
            uint8_t g6 = (c >> 5) & 0x3F;
            uint8_t b5 = c & 0x1F;
            /* 18 位模式：每字节 D7/D6 为 0，颜色 6 位在 D5-D0（手册 9.8.4） */
            s_row[x * 3 + 0] = (uint8_t)((r5 << 1) | (r5 >> 4));
            s_row[x * 3 + 1] = g6;
            s_row[x * 3 + 2] = (uint8_t)((b5 << 1) | (b5 >> 4));
        }
        lcd_set_window(0, y, s_w - 1, y);
        lcd_cmd(0x2C);
        lcd_data(s_row, (size_t)s_w * 3);
    }
    return ESP_OK;
}
