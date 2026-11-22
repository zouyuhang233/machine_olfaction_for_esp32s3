/*
 * font.h -- 点阵字库描述
 *
 * 一份字库 = 一张 95 字符（ASCII 32..126）的位图表 + 尺寸信息。
 * 位图格式统一：每字符 h 字节，1 字节 = 1 行像素，bit7 = 最左一列；
 * 字符 c 的数据偏移 = (c - 32) * h。
 *
 * 现有字库：
 *   font8x16  8 宽 16 高，厂商原始字库（正文字号）
 *   font8x12  8 宽 12 高，由 8x16 降采样而来（小一号，横屏一行 20 字符）
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *name;
    uint8_t w;          ///< 字形宽度（像素）
    uint8_t h;          ///< 字形高度（像素），也是每个字符的字节数
    uint8_t advance;    ///< 行进宽度（像素），通常等于 w
    const uint8_t *data;
} lcd_font_t;

extern const lcd_font_t font8x16;
extern const lcd_font_t font8x12;

/** 取字符位图；超出 ASCII 32..126 时退回 '?'。 */
static inline const uint8_t *font_glyph(const lcd_font_t *f, char c)
{
    unsigned ch = (unsigned char)c;
    if (ch < 32 || ch > 126) {
        ch = '?';
    }
    return f->data + (size_t)(ch - 32) * f->h;
}

#ifdef __cplusplus
}
#endif
