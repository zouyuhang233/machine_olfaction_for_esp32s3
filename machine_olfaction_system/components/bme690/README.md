# BME690 采集组件

## 接线

| BME690 扩展板 | ESP32-S3 | 说明 |
|---|---|---|
| VDD / VDDIO | 3V3 | 3.3 V |
| GND | GND | 共地 |
| CSB | VDDIO | 必须直连；拉低一次会锁到 SPI，只能断电恢复 |
| SDO | GND | I²C 7 位地址 0x76 |
| SDI | GPIO8 | SDA，外部 4.7 kΩ 上拉到 VDDIO |
| SCK | GPIO18 | SCL，外部 4.7 kΩ 上拉到 VDDIO |

## 启动自检日志

正常时应看到：

```text
I (xxx) bme690: I2C 就绪: SDA=GPIO8 SCL=GPIO18 100000 Hz addr=0x76
I (xxx) bme690: 身份: Chip_Id=0x61 Variant_Id=0x02 (BME690)
I (xxx) bme690: 初始化完成: 16x T/P/H，forced 模式，校准参数已加载
I (xxx) services: BME690 环境自检: 25.xx°C, 5x.xx%RH, 101x.xx hPa
I (xxx) services: BME690 #1: ... Rs=... kΩ gas-valid/heat-stable
```

初始环境自检不加热；之后每 1 s 做一次 300 °C / 100 ms 的 forced 测量。

## 失败提示

- `0x76 无应答`：先查 VDD/VDDIO/GND、GPIO8/18 是否接反、SCK/SDI 是否有 4.7 kΩ 上拉。
- `Chip_Id=0x61` 但 `Variant_Id` 不是 `0x02`：不是 BME690。
- 之前能读、现在扫不到：CSB 可能被瞬间拉低，断电重新上电并确认 CSB 直连 VDDIO。
- `Rs=--`：温湿度正常但气体测量无效，检查加热配置、供电和传感器是否刚上电未稳定。

## 数据约定

- BME690 不直接输出 TVOC/eCO2；服务层在没有 BSEC 时把这两项设为 `NAN`，UI 显示 `--`。
- `bme690_sample_t.temperature_c` 单位是 °C，`humidity_pct` 单位是 %RH；启用 `BME69X_USE_FPU` 时 Bosch API 已直接输出这两个单位，禁止再次除以 100/1000。
- `sensor_data_t.rs` 单位是 kΩ。
- 原始 `bme690_sample_t.gas_resistance_ohm` 单位是 Ω。

## 第三方代码

`bosch/` 是从 Bosch Sensortec `BME690_SensorAPI` 引入的 v1.1.0（commit `8a4d9229aaa26c960ca8f7717cb93087f067ecbe`），BSD-3-Clause，许可证见同目录 `LICENSE`。
