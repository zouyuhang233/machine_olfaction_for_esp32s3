/*
 * bsp.h -- 板级支持：只管"线怎么连、怎么拉电平"
 *
 * 边界：
 *   bsp  知道引脚、SPI 总线、背光；不知道屏的寄存器，也不知道画什么
 *   lcd  知道 ST7789 的命令、窗口、像素格式；不知道引脚号
 *   main 只负责按顺序调用
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 配置 DC/RST/BL 为输出，并初始化 SPI2 总线（只初始化总线，不挂设备）。 */
esp_err_t bsp_init(void);

/** 在 SPI 总线上挂一个屏设备（CS 用 board_pins.h 里的定义），返回设备句柄。 */
esp_err_t bsp_lcd_add_device(spi_device_handle_t *out_dev);

/** DC 电平：data=true 表示发数据，false 表示发命令。 */
void bsp_lcd_dc(bool data);

/** 硬件复位：RST 低 100 ms，高 50 ms。 */
void bsp_lcd_reset(void);

/** 背光开关（模块 BLK 脚高电平点亮、低电平关闭）。 */
void bsp_backlight(bool on);

/** 背光亮度 0..100（LEDC PWM，100 = 最亮）。 */
void bsp_backlight_set(uint8_t percent);

#ifdef __cplusplus
}
#endif
