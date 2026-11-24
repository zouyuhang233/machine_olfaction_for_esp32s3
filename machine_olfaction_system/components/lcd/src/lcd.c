/*
 * lcd.c -- ST7789 1.54" 240x240 SPI driver (RGB565)
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

/* ST7789 MADCTL: 0, 90, 180, 270 degrees. The panel is square. */
static const uint8_t s_madctl[4] = {0x00, 0x60, 0xC0, 0xA0};
/* 1.54-inch 240x240 panel occupies 240x320 controller RAM. */
static const int16_t s_xoff_table[4] = {0, 80, 0, 80};
static const int16_t s_yoff_table[4] = {80, 0, 80, 0};

#define FB_MAX_W 240
#define FB_MAX_H 240

static spi_device_handle_t s_dev;
static uint16_t *s_fb;
static uint8_t s_row[FB_MAX_W * 2];
static int s_w = BOARD_LCD_WIDTH;
static int s_h = BOARD_LCD_HEIGHT;
static int s_rot;
static int s_xoff;
static int s_yoff;
static bool s_inited;

static void lcd_cmd(uint8_t cmd)
{
    spi_transaction_t t = {.length = 8, .tx_buffer = &cmd};
    bsp_lcd_dc(false);
    ESP_ERROR_CHECK(spi_device_polling_transmit(s_dev, &t));
}

static void lcd_data(const uint8_t *buf, size_t len)
{
    if (len == 0) return;
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
    x0 += s_xoff;
    x1 += s_xoff;
    y0 += s_yoff;
    y1 += s_yoff;
    uint8_t caset[4] = {(uint8_t)(x0 >> 8), (uint8_t)x0, (uint8_t)(x1 >> 8), (uint8_t)x1};
    uint8_t raset[4] = {(uint8_t)(y0 >> 8), (uint8_t)y0, (uint8_t)(y1 >> 8), (uint8_t)y1};
    lcd_cmd_data(0x2A, caset, 4);
    lcd_cmd_data(0x2B, raset, 4);
}

static void lcd_init_sequence(void)
{
    static const uint8_t colmod[] = {0x55};
    static const uint8_t porctrl[] = {0x0C, 0x0C, 0x00, 0x33, 0x33};
    static const uint8_t gctrl[] = {0x35};
    static const uint8_t vcoms[] = {0x19};
    static const uint8_t lcmctrl[] = {0x2C};
    static const uint8_t vdvrvhen[] = {0x01};
    static const uint8_t vrhs[] = {0x12};
    static const uint8_t vdvs[] = {0x20};
    static const uint8_t frctrl2[] = {0x0F};
    static const uint8_t pwctrl1[] = {0xA4, 0xA1};
    static const uint8_t gamma_p[] = {0xD0, 0x04, 0x0D, 0x11, 0x13, 0x2B, 0x3F, 0x54,
                                      0x4C, 0x18, 0x0D, 0x0B, 0x1F, 0x23};
    static const uint8_t gamma_n[] = {0xD0, 0x04, 0x0C, 0x11, 0x13, 0x2C, 0x3F, 0x44,
                                      0x51, 0x2F, 0x1F, 0x1F, 0x20, 0x23};

    lcd_cmd(0x01);
    vTaskDelay(pdMS_TO_TICKS(120));
    lcd_cmd(0x11);
    vTaskDelay(pdMS_TO_TICKS(120));
    lcd_cmd_data(0x3A, colmod, sizeof(colmod));
    lcd_cmd_data(0x36, &s_madctl[0], 1);
    lcd_cmd_data(0xB2, porctrl, sizeof(porctrl));
    lcd_cmd_data(0xB7, gctrl, sizeof(gctrl));
    lcd_cmd_data(0xBB, vcoms, sizeof(vcoms));
    lcd_cmd_data(0xC0, lcmctrl, sizeof(lcmctrl));
    lcd_cmd_data(0xC2, vdvrvhen, sizeof(vdvrvhen));
    lcd_cmd_data(0xC3, vrhs, sizeof(vrhs));
    lcd_cmd_data(0xC4, vdvs, sizeof(vdvs));
    lcd_cmd_data(0xC6, frctrl2, sizeof(frctrl2));
    lcd_cmd_data(0xD0, pwctrl1, sizeof(pwctrl1));
    lcd_cmd_data(0xE0, gamma_p, sizeof(gamma_p));
    lcd_cmd_data(0xE1, gamma_n, sizeof(gamma_n));
    lcd_cmd(0x21);
    lcd_cmd(0x13);
    lcd_cmd(0x29);
    vTaskDelay(pdMS_TO_TICKS(20));
}

esp_err_t lcd_init(void)
{
    if (s_inited) return ESP_OK;
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
    s_xoff = s_xoff_table[0];
    s_yoff = s_yoff_table[0];
    s_w = BOARD_LCD_WIDTH;
    s_h = BOARD_LCD_HEIGHT;
    s_inited = true;
    ESP_LOGI(TAG, "ST7789 就绪: 240x240, RGB565, offset=(%d,%d)", s_xoff, s_yoff);
    return ESP_OK;
}

esp_err_t lcd_set_rotation(int rot)
{
    if (!s_inited) return ESP_ERR_INVALID_STATE;
    rot &= 3;
    lcd_cmd_data(0x36, &s_madctl[rot], 1);
    s_rot = rot;
    s_xoff = s_xoff_table[rot];
    s_yoff = s_yoff_table[rot];
    s_w = BOARD_LCD_WIDTH;
    s_h = BOARD_LCD_HEIGHT;
    ESP_LOGI(TAG, "旋转 %d: %dx%d, MADCTL=0x%02X, offset=(%d,%d)",
             s_rot, s_w, s_h, s_madctl[rot], s_xoff, s_yoff);
    return ESP_OK;
}

int lcd_width(void) { return s_w; }
int lcd_height(void) { return s_h; }

void lcd_draw_pixel(int x, int y, uint16_t color)
{
    if (s_fb == NULL || x < 0 || x >= s_w || y < 0 || y >= s_h) return;
    s_fb[(size_t)y * s_w + x] = color;
}

void lcd_clear(uint16_t color)
{
    if (s_fb == NULL) return;
    for (size_t i = 0; i < (size_t)s_w * s_h; i++) s_fb[i] = color;
}

void lcd_fill_rect(int x, int y, int w, int h, uint16_t color)
{
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) lcd_draw_pixel(x + i, y + j, color);
    }
}

void lcd_draw_char(const lcd_font_t *font, int x, int y, char c, uint16_t fg, uint16_t bg)
{
    if (font == NULL) return;
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
    if (font == NULL || s == NULL) return 0;
    int cx = x;
    for (const char *p = s; *p != '\0'; p++) {
        lcd_draw_char(font, cx, y, *p, fg, bg);
        cx += font->advance;
    }
    return cx - x;
}

void lcd_draw_text_center(const lcd_font_t *font, int cx, int y, const char *s, uint16_t fg, uint16_t bg)
{
    if (font == NULL || s == NULL) return;
    int w = (int)strlen(s) * font->advance;
    lcd_draw_text(font, cx - w / 2, y, s, fg, bg);
}

static void lcd_pack_rgb565(uint16_t c, uint8_t *dst)
{
    dst[0] = (uint8_t)(c >> 8);
    dst[1] = (uint8_t)(c & 0xFF);
}

esp_err_t lcd_flush(void)
{
    if (!s_inited || s_fb == NULL) return ESP_ERR_INVALID_STATE;
    for (int y = 0; y < s_h; y++) {
        const uint16_t *src = &s_fb[(size_t)y * s_w];
        for (int x = 0; x < s_w; x++) lcd_pack_rgb565(src[x], &s_row[x * 2]);
        lcd_set_window(0, y, s_w - 1, y);
        lcd_cmd(0x2C);
        lcd_data(s_row, (size_t)s_w * 2);
    }
    return ESP_OK;
}

esp_err_t lcd_flush_area(int x1, int y1, int x2, int y2, const uint16_t *pixels)
{
    if (!s_inited) return ESP_ERR_INVALID_STATE;
    if (pixels == NULL) return ESP_ERR_INVALID_ARG;
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= s_w) x2 = s_w - 1;
    if (y2 >= s_h) y2 = s_h - 1;
    if (x1 > x2 || y1 > y2) return ESP_ERR_INVALID_ARG;
    const int w = x2 - x1 + 1;
    for (int y = y1; y <= y2; y++) {
        const uint16_t *src = pixels + (size_t)(y - y1) * w;
        for (int x = 0; x < w; x++) lcd_pack_rgb565(src[x], &s_row[x * 2]);
        lcd_set_window(x1, y, x2, y);
        lcd_cmd(0x2C);
        lcd_data(s_row, (size_t)w * 2);
    }
    return ESP_OK;
}

static const uint8_t s_bayer4[4][4] = {
    {0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5},
};

static inline uint8_t dither_to_bits(uint8_t v, uint8_t threshold, uint8_t max)
{
    uint16_t t = (uint16_t)v * max + threshold;
    uint16_t q = t / 255u;
    return (uint8_t)(q > max ? max : q);
}

esp_err_t lcd_flush_area_rgb8888(int x1, int y1, int x2, int y2, const uint32_t *pixels)
{
    if (!s_inited) return ESP_ERR_INVALID_STATE;
    if (pixels == NULL) return ESP_ERR_INVALID_ARG;
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= s_w) x2 = s_w - 1;
    if (y2 >= s_h) y2 = s_h - 1;
    if (x1 > x2 || y1 > y2) return ESP_ERR_INVALID_ARG;
    const int w = x2 - x1 + 1;
    for (int y = y1; y <= y2; y++) {
        const uint32_t *src = pixels + (size_t)(y - y1) * w;
        for (int x = 0; x < w; x++) {
            uint32_t v = src[x];
            uint8_t r8 = (uint8_t)((v >> 16) & 0xFF);
            uint8_t g8 = (uint8_t)((v >> 8) & 0xFF);
            uint8_t b8 = (uint8_t)(v & 0xFF);
            uint8_t th = (uint8_t)(s_bayer4[y & 3][x & 3] * 17);
            uint16_t r5 = dither_to_bits(r8, th, 31);
            uint16_t g6 = dither_to_bits(g8, th, 63);
            uint16_t b5 = dither_to_bits(b8, th, 31);
            lcd_pack_rgb565((uint16_t)((r5 << 11) | (g6 << 5) | b5), &s_row[x * 2]);
        }
        lcd_set_window(x1, y, x2, y);
        lcd_cmd(0x2C);
        lcd_data(s_row, (size_t)w * 2);
    }
    return ESP_OK;
}
