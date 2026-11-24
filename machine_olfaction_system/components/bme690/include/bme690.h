/*
 * bme690.h -- BME690 驱动边界
 *
 * 只负责：I²C、芯片自检、Bosch 补偿、forced 模式单点测量。
 * 不负责：任务调度、UI、Modbus、BSEC。
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BME690_DEFAULT_HEATER_TEMP_C     300
#define BME690_DEFAULT_HEATER_DURATION_MS 100
#define BME690_SAMPLE_PERIOD_MS          1000

typedef struct {
    bool valid;
    bool gas_valid;
    bool heat_stable;
    uint8_t chip_id;
    uint8_t variant_id;
    int8_t bosch_status;

    float temperature_c;
    float humidity_pct;
    float pressure_hpa;
    float gas_resistance_ohm;

    uint32_t measurement_us;
    int64_t timestamp_us;
} bme690_sample_t;

/** 安装 I²C、验证身份、软复位并加载校准参数。失败后可以重复调用重试。 */
esp_err_t bme690_init(void);

/** 芯片是否已经初始化完成。 */
bool bme690_ready(void);

/** 无加热测量温湿度/气压，用于接线和身份验证。 */
esp_err_t bme690_read_environment(bme690_sample_t *out);

/** 单点加热测量，返回温湿度/气压和 Rs。 */
esp_err_t bme690_read_gas(bme690_sample_t *out,
                          uint16_t heater_temp_c,
                          uint16_t heater_duration_ms);

uint8_t bme690_chip_id(void);
uint8_t bme690_variant_id(void);
int8_t bme690_last_status(void);
const char *bme690_variant_name(uint8_t variant_id);
const char *bme690_status_text(int8_t status);

#ifdef __cplusplus
}
#endif
