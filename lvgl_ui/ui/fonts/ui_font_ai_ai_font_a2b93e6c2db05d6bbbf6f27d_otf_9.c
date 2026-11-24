/*******************************************************************************
 * Size: 9 px
 * Bpp: 4
 * Opts: AiDoGUI Designer JavaScript API
 ******************************************************************************/

#ifdef __has_include
    #if __has_include("lvgl.h")
        #ifndef LV_LVGL_H_INCLUDE_SIMPLE
            #define LV_LVGL_H_INCLUDE_SIMPLE
        #endif
    #endif
#endif

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
    #include "lvgl.h"
#else
    #include "lvgl.h"
#endif

#ifndef UI_FONT_AI_AI_FONT_A2B93E6C2DB05D6BBBF6F27D_OTF_9
#define UI_FONT_AI_AI_FONT_A2B93E6C2DB05D6BBBF6F27D_OTF_9 1
#endif

#if UI_FONT_AI_AI_FONT_A2B93E6C2DB05D6BBBF6F27D_OTF_9

#if !(LV_VERSION_CHECK(9, 3, 0) || LVGL_VERSION_MAJOR >= 10)
#error "At least LVGL v9.3 is required to use the stride attribute of the fonts"
#endif

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+54CD "响" */
    0x00, 0x00, 0x06, 0x00, 0x00,
    0x59, 0x70, 0x2b, 0x00, 0x00,
    0x80, 0x9a, 0x88, 0x8c, 0x20,
    0x80, 0x98, 0x37, 0x67, 0x20,
    0x80, 0x98, 0x70, 0x77, 0x20,
    0x85, 0x98, 0x70, 0x77, 0x20,
    0x85, 0x38, 0x78, 0x67, 0x20,
    0x00, 0x08, 0x00, 0x07, 0x20,
    0x00, 0x08, 0x00, 0x3b, 0x10,
    0x00, 0x00, 0x00, 0x00, 0x00,

    /* U+5E94 "应" */
    0x00, 0x00, 0x71, 0x00, 0x00,
    0x08, 0x99, 0xcb, 0x99, 0x40,
    0x0a, 0x00, 0x20, 0x03, 0x00,
    0x0a, 0x61, 0x82, 0x0b, 0x00,
    0x0a, 0x37, 0x36, 0x37, 0x00,
    0x0a, 0x0a, 0x08, 0x91, 0x00,
    0x28, 0x03, 0x01, 0x90, 0x00,
    0x64, 0x99, 0x9c, 0xb9, 0x50,
    0x30, 0x00, 0x00, 0x00, 0x00,

    /* U+5EA6 "度" */
    0x00, 0x00, 0x50, 0x00, 0x00,
    0x08, 0x99, 0xdb, 0x99, 0x40,
    0x0a, 0x08, 0x00, 0x80, 0x00,
    0x0a, 0x8d, 0x88, 0xd8, 0x30,
    0x0a, 0x09, 0x77, 0xc0, 0x00,
    0x0a, 0x78, 0x88, 0x85, 0x00,
    0x18, 0x08, 0x52, 0xb2, 0x00,
    0x64, 0x36, 0xdd, 0x84, 0x10,
    0x31, 0x52, 0x00, 0x15, 0x30,

    /* U+6C14 "气" */
    0x00, 0x50, 0x00, 0x00, 0x00,
    0x05, 0xc9, 0x99, 0x99, 0x00,
    0x0a, 0x44, 0x44, 0x42, 0x00,
    0x74, 0x34, 0x44, 0x41, 0x00,
    0x05, 0x99, 0x99, 0xb0, 0x00,
    0x00, 0x00, 0x00, 0xa0, 0x00,
    0x00, 0x00, 0x00, 0xa0, 0x00,
    0x00, 0x00, 0x00, 0x93, 0x40,
    0x00, 0x00, 0x00, 0x2c, 0x20,

    /* U+6E29 "温" */
    0x01, 0x00, 0x00, 0x00, 0x00,
    0x19, 0x5c, 0x99, 0xa7, 0x00,
    0x00, 0x0c, 0x88, 0x97, 0x00,
    0x77, 0x0b, 0x44, 0x67, 0x00,
    0x03, 0x04, 0x44, 0x42, 0x00,
    0x01, 0x5c, 0xca, 0xaa, 0x00,
    0x08, 0x47, 0x83, 0x49, 0x00,
    0x19, 0x17, 0x83, 0x49, 0x00,
    0x63, 0x9c, 0xcb, 0xbd, 0x30,
    0x00, 0x00, 0x00, 0x00, 0x00,

    /* U+6E7F "湿" */
    0x01, 0x00, 0x00, 0x00, 0x00,
    0x19, 0x4d, 0x88, 0x8b, 0x00,
    0x00, 0x0c, 0x88, 0x8b, 0x00,
    0x87, 0x0a, 0x00, 0x0a, 0x00,
    0x06, 0x08, 0x88, 0x86, 0x00,
    0x00, 0x01, 0x71, 0x52, 0x00,
    0x08, 0x38, 0x92, 0x7a, 0x00,
    0x19, 0x08, 0xa2, 0x96, 0x00,
    0x63, 0x89, 0xda, 0xc9, 0x30,
    0x00, 0x00, 0x00, 0x00, 0x00,

    /* U+963B "阻" */
    0x00, 0x00, 0x00, 0x00,
    0xdb, 0x8b, 0x99, 0xd0,
    0x98, 0x19, 0x00, 0xa0,
    0xab, 0x0b, 0x99, 0xd0,
    0x93, 0x69, 0x00, 0xa0,
    0xa1, 0x8b, 0x88, 0xd0,
    0xb8, 0x1a, 0x00, 0xa0,
    0x90, 0x09, 0x00, 0xa0,
    0x90, 0x79, 0x99, 0x97
};

/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 144, .box_w = 9, .box_h = 10, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 50, .adv_w = 144, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 95, .adv_w = 144, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 140, .adv_w = 144, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 185, .adv_w = 144, .box_w = 9, .box_h = 10, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 235, .adv_w = 144, .box_w = 9, .box_h = 10, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 285, .adv_w = 144, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = -1}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/

static const uint16_t unicode_list_0[] = {
    0x00, 0x9c7, 0x9d9, 0x1747, 0x195c, 0x19b2, 0x416e
};

/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 21709, .range_length = 16751, .glyph_id_start = 1,
        .unicode_list = unicode_list_0, .glyph_id_ofs_list = NULL, .list_length = 7, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY
    }
};



/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = 4,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
    .stride = 1
};

extern const lv_font_t lv_font_montserrat_14;


/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t ui_font_ai_ai_font_a2b93e6c2db05d6bbbf6f27d_otf_9 = {
#else
lv_font_t ui_font_ai_ai_font_a2b93e6c2db05d6bbbf6f27d_otf_9 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 13,          /*The maximum line height required by the font*/
    .base_line = 3,             /*Baseline measured from the bottom of the line*/
#if LV_VERSION_CHECK(9, 6, 0) || LVGL_VERSION_MAJOR >= 10
    .cap_height = 7,           /*Cap height of the font*/
    .x_height = 5,               /*x-height of the font*/
#endif
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -1,
    .underline_thickness = 0,
#endif

#if LV_VERSION_CHECK(9, 3, 0) || LVGL_VERSION_MAJOR >= 10
    .static_bitmap = 1,    /*Bitmaps are stored as const so they are always static if not compressed */
#endif

    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = &lv_font_montserrat_14,
#endif
    .user_data = NULL,
};



#endif /*#if UI_FONT_AI_AI_FONT_A2B93E6C2DB05D6BBBF6F27D_OTF_9*/
