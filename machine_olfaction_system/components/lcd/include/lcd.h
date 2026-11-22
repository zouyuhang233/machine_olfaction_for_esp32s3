/*
 * lcd.h -- ST7735S 显示驱动（128x160，可旋转为 160x128 横屏）
 *
 * 只管"屏幕怎么初始化、像素怎么写"，不认识页面/布局。
 * 上层用法：lcd_init() → lcd_set_rotation(1) → lcd_clear()/lcd_draw_text() … → lcd_flush()
 *
 * 重要实现细节（踩坑得来，别改）：
 *   1. 这块模组的窗口偏移是 (0,0)：CASET/RASET 直接写 0..127 / 0..159
 *   2. 面板实际按 18 位（RGB666，3 字节/像素）解释数据；写 16 位会出现
 *      "字被沿对角线拉弯、3 行一循环"的现象 → 所以 0x3A=0x06 + 发送端展开成 3 字节
 *   3. 初始化序列照厂商 CTC180 例程，全程不发反显命令
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "font.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 常用颜色（RGB565） */
#define LCD_BLACK   0x0000
#define LCD_WHITE   0xFFFF
#define LCD_RED     0xF800
#define LCD_GREEN   0x07E0
#define LCD_BLUE    0x001F
#define LCD_YELLOW  0xFFE0
#define LCD_CYAN    0x07FF
#define LCD_MAGENTA  0xF81F

#define LCD_RGB565(r, g, b) \
    ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))

/** 初始化：挂 SPI 设备 → 复位 → 初始化序列 → 分配帧缓冲。 */
esp_err_t lcd_init(void);

/**
 * 旋转：
 *   0 = 竖屏 128x160（(0,0) 左上）
 *   1 = 横屏 160x128（右转 90°）
 *   2 = 竖屏 128x160（倒置）
 *   3 = 横屏 160x128（左转 90°）
 */
esp_err_t lcd_set_rotation(int rot);

/** 当前方向下的宽/高。 */
int lcd_width(void);
int lcd_height(void);

/* ---------------- 画到帧缓冲（最后统一 lcd_flush） ---------------- */
void lcd_clear(uint16_t color);
void lcd_draw_pixel(int x, int y, uint16_t color);
void lcd_fill_rect(int x, int y, int w, int h, uint16_t color);

/** 画一个字符（font 决定字号）。 */
void lcd_draw_char(const lcd_font_t *font, int x, int y, char c, uint16_t fg, uint16_t bg);

/** 画字符串，返回占用宽度。 */
int lcd_draw_text(const lcd_font_t *font, int x, int y, const char *s, uint16_t fg, uint16_t bg);

/** 以 cx 为水平中心画一行字。 */
void lcd_draw_text_center(const lcd_font_t *font, int cx, int y, const char *s, uint16_t fg, uint16_t bg);

/** 把帧缓冲推到屏上（逐行 18 位发送）。 */
esp_err_t lcd_flush(void);

#ifdef __cplusplus
}
#endif
