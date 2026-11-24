# components/bme690 —— BME690 驱动

## 组成

| 文件 | 说明 |
|---|---|
| `include/bme690.h` | 本项目使用的 API |
| `src/bme690.c` | ESP-IDF I²C 适配 + 强制模式单步测量 + 并行模式多步剖面 |
| `bosch/bme69x.c/.h/bme69x_defs.h` | Bosch 官方 **BME69X SensorAPI v1.1.0**（BSD-3-Clause，未改动算法） |
| `bosch/LICENSE` | 官方许可证原文 |

## 依赖

- ESP-IDF v5.x（旧版 `driver/i2c.h` 主机 API，5.1.2 与 5.5.6 均可用）
- 一个 I²C 主机端口（默认 `I2C_NUM_0`）

## 默认接线

见 `../bsp/include/board_pins.h`：

| BME690 | ESP32-S3 |
|---|---|
| SDI (SDA) | GPIO8 |
| SCK (SCL) | GPIO18 |
| CSB | VDDIO（直连，0Ω/跳线） |
| SDO | GND（地址 0x76） |

## 使用

```c
#include "bme690.h"

ESP_ERROR_CHECK(bme690_init(I2C_NUM_0, GPIO_NUM_8, GPIO_NUM_18, 0x76));

bme690_reading_t r;
if (bme690_read(&r) == ESP_OK) {
    printf("T=%.2f H=%.2f P=%.1f Rs=%.0f status=0x%02X\n",
           r.temperature_c, r.humidity_pct, r.pressure_pa,
           r.gas_resistance_ohm, r.status);
}
```

多步加热剖面：

```c
uint16_t temps[10] = {320, 100, 100, 100, 200, 200, 200, 320, 320, 320};
uint16_t durs[10]  = { 50,  50,  50,  50,  50,  50,  50,  50,  50,  50};
bme690_set_profile(temps, durs, 10, 0);
bme690_reading_t out[10];
uint8_t n;
bme690_read_profile(out, 10, &n);
```

## 注意

- 外部 **4.7k 上拉**（SDA/SCL 到 VDDIO）必须有；芯片内部无上拉。
- **CSB 直连 VDDIO**，不能串电阻；被拉低一次后 I²C 锁死到下次重新上电。
- 完整调试步骤见 `器件文档/BME690/BME690_使用与调试指南.md`。
