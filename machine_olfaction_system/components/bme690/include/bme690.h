/*
 * bme690.h -- BME690 四合一环境/气体传感器（ESP-IDF I²C 封装）
 *
 * 底层用 Bosch 官方 BME69X SensorAPI（BSD-3，见 bosch/），这里只做三件事：
 *   1. 把 ESP-IDF 的 I²C 读写/延时接到 API 的函数指针；
 *   2. 提供"一次强制模式测量"的简单接口；
 *   3. 提供多步加热剖面（parallel mode）的采集接口，供后续温度调制使用。
 *
 * 默认接线（见 board_pins.h）：
 *   SDA = GPIO8, SCL = GPIO18, 地址 0x76（SDO 接 GND）
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/i2c.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 一次测量的结果（已补偿）。 */
typedef struct {
    float temperature_c;      /* 摄氏度 */
    float pressure_pa;        /* 帕斯卡 */
    float humidity_pct;       /* 相对湿度 % */
    float gas_resistance_ohm; /* 气体电阻 Rs，欧姆 */
    uint8_t status;           /* bit7 new_data, bit5 gas_valid, bit4 heat_stab */
    uint8_t gas_index;        /* 当前数据对应的加热步骤号 0..9 */
} bme690_reading_t;

/**
 * 初始化 I²C 总线 + 传感器。
 *
 * @param port      I2C 端口（I2C_NUM_0 / I2C_NUM_1）
 * @param sda_io    SDA GPIO
 * @param scl_io    SCL GPIO
 * @param i2c_addr  7 位地址：0x76（SDO→GND）或 0x77（SDO→VDDIO）
 *
 * @return ESP_OK 成功；ESP_ERR_NOT_FOUND 表示总线上没有响应；
 *         ESP_FAIL 表示芯片有响应但初始化/自检失败。
 */
esp_err_t bme690_init(i2c_port_t port, gpio_num_t sda_io, gpio_num_t scl_io, uint8_t i2c_addr);

/** 是否已初始化成功。 */
bool bme690_ready(void);

/** 最近一次底层 API 返回码（调试用）。 */
int8_t bme690_last_rslt(void);

/** 把底层返回码翻译成可读字符串。 */
const char *bme690_rslt_name(int8_t rslt);

/** 芯片 ID（0xD0，BME690/688/680 都是 0x61）。 */
uint8_t bme690_chip_id(void);

/** 型号 ID（0xF0，BME690 应为 0x02）。 */
uint8_t bme690_variant_id(void);

/** 打印关键寄存器（chip_id / variant_id / ctrl_hum / ctrl_meas / config / ctrl_gas_1）。 */
esp_err_t bme690_log_registers(void);

/**
 * 设置单步加热（强制模式）的温度与持续时间。
 * 修改后下一次 bme690_read() 生效。
 */
esp_err_t bme690_set_heater(uint16_t temp_c, uint16_t duration_ms);

/**
 * 强制模式读一次：温度、压力、湿度、气体电阻。
 * 阻塞约 (TPH 测量时间 + duration_ms)。
 */
esp_err_t bme690_read(bme690_reading_t *out);

/**
 * 设置多步加热剖面（并行模式）。
 *
 * @param temps_c          各步目标温度（°C），最多 10 步
 * @param durations_ms     各步加热保持时间（ms），最多 10 步
 * @param count            步数 1..10
 * @param shared_duration_ms 每个 TPHG 序列前的公共等待（ms）；传 0 表示自动按 140ms 周期计算
 */
esp_err_t bme690_set_profile(const uint16_t *temps_c, const uint16_t *durations_ms,
                             uint8_t count, uint16_t shared_duration_ms);

/**
 * 执行一次完整加热剖面采集。
 *
 * @param out        输出数组，按 gas_index 排列（out[0] 对应第 0 步）
 * @param max_count  out 数组容量（>= 剖面步数）
 * @param out_count  实际采到的步数
 *
 * @note 内部会启动并行模式并在采集完成后回到睡眠模式；阻塞时间约
 *       count * (shared_duration_ms + TPH 测量时间)。
 */
esp_err_t bme690_read_profile(bme690_reading_t *out, uint8_t max_count, uint8_t *out_count);

#ifdef __cplusplus
}
#endif
