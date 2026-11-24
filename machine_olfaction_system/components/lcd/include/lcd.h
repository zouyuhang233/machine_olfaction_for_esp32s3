/*
 * lcd.h -- ST7789 1.54" 显示驱动（240x240，RGB565）
 *
 * 只管"屏幕怎么初始化、像素怎么写"，不认识页面/布局。
 * 上层用法：lcd_init() → lcd_set_rotation(0) → LVGL flush
 *
 * 重要实现细节（踩坑得来，别改）：
 *   1. 该 YW-T114 模组使用原生 240×240 窗口，所有方向 offset=(0,0)
 *   2. 使用 RGB565，每像素 2 字节，COLMOD=0x55
 *   3. BLK 高电平点亮、低电平关闭；背光 PWM 在 bsp 层按高有效处理
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
 *   0 = 0°（240x240）
 *   1 = 90°（240x240）
 *   2 = 180°（240x240）
 *   3 = 270°（240x240）
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

/** 把帧缓冲推到屏上（逐行 RGB565 发送）。 */
esp_err_t lcd_flush(void);

/**
 * 直接把一块 RGB565 像素区域刷到屏上。
 * pixels 按行优先排列，大小为 (x2-x1+1) * (y2-y1+1)。
 */
esp_err_t lcd_flush_area(int x1, int y1, int x2, int y2, const uint16_t *pixels);

/**
 * 直接把一块 XRGB8888 像素区域刷到屏上（LVGL 32 位色用）。
 * pixels 按行优先排列，每像素 4 字节，内存布局 B,G,R,X（小端）。
 * 内部做 RGB565 抖动后发送。
 */
esp_err_t lcd_flush_area_rgb8888(int x1, int y1, int x2, int y2, const uint32_t *pixels);

#ifdef __cplusplus
}
#endif
