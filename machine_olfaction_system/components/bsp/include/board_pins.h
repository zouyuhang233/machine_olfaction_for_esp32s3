/*
 * board_pins.h -- 这块板子的引脚事实（唯一来源）
 *
 * 换线只改这个文件。当前接法（1.54" ST7789 240x240，8 脚模块）：
 *
 *   屏 GND -> GND        屏 VCC -> 3V3
 *   屏 SCL -> GPIO12     屏 SDA -> GPIO11
 *   屏 RST -> GPIO14     屏 DC  -> GPIO9
 *   屏 CS  -> GPIO10     屏 BLK -> GPIO13
 *   BLK 为高电平点亮、低电平关闭，驱动层已按高有效 PWM 处理。
 *
 * 新屏没有板载按键，使用 4 个外接按键：
 *   LEFT / RIGHT / OK / BACK
 *   GPIO21 / GPIO38 不再使用，也不初始化。
 */
#pragma once

#include "driver/gpio.h"
#include "driver/spi_master.h"

/* ---------------- 1.54" ST7789 240x240 ---------------- */
#define BOARD_LCD_SPI_HOST  SPI2_HOST
#define BOARD_LCD_SCLK      GPIO_NUM_12
#define BOARD_LCD_MOSI      GPIO_NUM_11
#define BOARD_LCD_CS        GPIO_NUM_10
#define BOARD_LCD_DC        GPIO_NUM_9
#define BOARD_LCD_RST       GPIO_NUM_14
#define BOARD_LCD_BL        GPIO_NUM_13

#define BOARD_LCD_WIDTH     240
#define BOARD_LCD_HEIGHT    240

/* ST7789 支持高速 SPI；40 MHz 兼顾动画刷新和飞线稳定性。
 * 若杜邦线较长出现花屏，依次降到 30 / 20 / 15 MHz。 */
#define BOARD_LCD_SPI_HZ    40000000

/* ---------------- BME690 I²C ---------------- */
#define BOARD_BME690_I2C_PORT 0
#define BOARD_BME690_I2C_SDA  GPIO_NUM_8
#define BOARD_BME690_I2C_SCL  GPIO_NUM_18
#define BOARD_BME690_I2C_ADDR 0x76
#define BOARD_BME690_I2C_HZ   100000

/* ---------------- 4 键（按下拉低到 GND） ---------------- */
#define BOARD_KEY_LEFT      GPIO_NUM_6
#define BOARD_KEY_RIGHT     GPIO_NUM_7
#define BOARD_KEY_OK        GPIO_NUM_4
#define BOARD_KEY_BACK      GPIO_NUM_5
