/*
 * board_pins.h -- 这块板子的引脚事实（唯一来源）
 *
 * 换线只改这个文件。当前接法（1.8" ST7735S 128x160，12 脚模块）：
 *
 *   屏 GND -> GND        屏 VCC -> 3V3
 *   屏 SCL -> GPIO12     屏 SDA -> GPIO11
 *   屏 RST -> GPIO14     屏 DC  -> GPIO9
 *   屏 CS  -> GPIO10     屏 BLK -> GPIO13
 *
 *   BME690（I²C）: SDA -> GPIO8, SCL -> GPIO18, 地址 0x76（SDO->GND）
 *
 * 模块自带的 4 个按键（K1~K4）暂未使用。
 */
#pragma once

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"

/* ---------------- 屏幕 ---------------- */
#define BOARD_LCD_SPI_HOST  SPI2_HOST
#define BOARD_LCD_SCLK      GPIO_NUM_12
#define BOARD_LCD_MOSI      GPIO_NUM_11
#define BOARD_LCD_CS        GPIO_NUM_10
#define BOARD_LCD_DC        GPIO_NUM_9
#define BOARD_LCD_RST       GPIO_NUM_14
#define BOARD_LCD_BL        GPIO_NUM_13

#define BOARD_LCD_WIDTH     128
#define BOARD_LCD_HEIGHT    160

/* 面包板上先跑 1 MHz；线短了可以往 10~40 MHz 提 */
#define BOARD_LCD_SPI_HZ    1000000

/* ---------------- BME690（I²C） ---------------- */
#define BOARD_BME690_I2C_PORT   I2C_NUM_0
#define BOARD_BME690_SDA        GPIO_NUM_8
#define BOARD_BME690_SCL        GPIO_NUM_18
#define BOARD_BME690_I2C_ADDR   0x76   /* SDO→GND；SDO→VDDIO 时为 0x77 */
#define BOARD_BME690_I2C_HZ     100000 /* 先 100kHz 跑稳，再试 400kHz */
