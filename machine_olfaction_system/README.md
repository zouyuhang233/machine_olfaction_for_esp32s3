# machine_olfaction_system —— ESP32-S3 机器嗅觉固件

基于 **ESP-IDF v5.1.2** 的电子鼻固件工程。目标板：**ESP32-S3-WROOM-1 N16R8**（16MB Flash + 8MB Octal PSRAM），串口 COM7。

> 全项目上下文（选型、采购、硬件评审、调试计划、待办）见 **上级目录的 `MEMORY.md`**，先读那份。

## 当前状态

| 项 | 状态 |
|---|---|
| 编译 | ✅ 通过（2026-11-24，本机 ESP-IDF 5.5.6 验证，BME690 版本 `machine_olfaction_system.bin` **289,616 B / 0x46B50**；工程原定 5.1.2，代码使用 5.1 兼容 API） |
| 显示 | ✅ 横屏 160×128，ST7735S 18 位 RGB666；当前显示 BME690 的 T/H/P/Rs（启动/失败时显示 init/ERR 状态） |
| BME690 | 🟡 驱动/自检已写入（`components/bme690`），等待硬件实测；上电自动读 ID、温湿度、气压、300°C Rs 并显示 |
| 业务代码 | BME690 采集进行中；按键 / 推理 / Modbus ❌ 未开始 |
| 下一步 | BME690 硬件 bring-up（按 `器件文档\BME690\BME690_使用与调试指南.md`）→ 按键 K1–K4 → Modbus RTU 从站 |

## 目录

```
main/app_main.c      应用层：屏显 + BME690 每秒采集/自检
components/bsp/      引脚 / SPI 总线(DMA) / 背光 / 复位
components/lcd/      ST7735S 18 位驱动 + 旋转 + 帧缓冲 + 画点/画字
components/font/     8x16 / 8x12 ASCII 点阵字库
components/bme690/   BME690 驱动：Bosch BME69X SensorAPI + ESP-IDF I²C 适配
sdkconfig.defaults   配置权威：改配置改这里，再删 sdkconfig 重新 set-target
sdkconfig            生成物（已 gitignore）
partitions.csv       分区表：nvs 24K / phy_init 4K / factory 4M / storage ~11.7M(fat) / coredump 512K
build/               构建产物（已 gitignore）
```

## 构建（Windows / PowerShell）

工具链：`C:\Espressif\frameworks\esp-idf-v5.1.2`，Python env `C:\Espressif\python_env\idf5.1_py3.11_env`，
`xtensa-esp32s3-elf\esp-12.2.0_20230208`，cmake 3.24.0，ninja 1.10.2。

> 2026-11-24 本机验证使用 **ESP-IDF v5.5.6**（`D:\Espressif\v5.5.6\esp-idf`）；BME690 组件刻意只用 v5.1 也有的 legacy `driver/i2c.h` API，可在原 5.1.2 环境直接构建。

### 三个必须知道的坑

1. **不要用 `export.sh`**：检测到 MSys/Mingw 会直接报错拒绝执行。
2. **不要直接 `python tools/idf.py`**：环境里只要有 `MSYSTEM`，`idf.py` 顶部会只打警告并跳过 `main()`，表现为**命令静默无输出**。用包装脚本 `C:\Users\zyh\AppData\Local\Temp\idf_run.py`（内部 pop 掉 `MSYSTEM/MINGW_PREFIX/MSYS`，`sys.path.insert` 后 runpy 执行）。
3. **必须设 `CODEBUDDY_SAFE_DELETE_ENABLED=0`**：否则 safe-delete shim 拦截 `os.remove`，idf_component_manager 报 `SHFileOperationW 失败: 0x2`，构建在 bootloader 配置阶段中断。

> `C:\Users\zyh\AppData\Local\Temp\idf_run.py` 会随 Temp 清理消失。若不存在就按原逻辑重建：pop 掉 `MSYSTEM/MINGW_PREFIX/MSYS` → `sys.path.insert(0, IDF_PATH/tools)` → `runpy.run_path(IDF_PATH/tools/idf.py)`。

### 显示驱动的两个坑（2026-11-22 踩过）

1. **SPI 总线必须开 DMA**：ESP-IDF 在 CPU 模式下单次传输上限只有 512 bit（64 字节），横屏一行 `160×3=480` 字节会被驱动直接拒绝；`lcd_cmd/lcd_data` 已加 `ESP_ERROR_CHECK`，失败会直接报错而不是静默花屏。
2. **18 位像素格式**：ST7735S 复位后 COLMOD 默认 `0x06`（18 位/pixel，手册 10.1.33）。每像素 3 字节，每字节 D7/D6=0、颜色 6 位在 D5–D0（手册 9.8.4）；按 16 位发会出现「约 3 行一循环」的斜纹。

### BME690 接线与调试

- 接线：SDA=**GPIO8**、SCL=**GPIO18**、地址 **0x76**（SDO→GND）；CSB 直连 VDDIO，SDA/SCL 各 4.7k 上拉到 VDDIO，VDD/VDDIO 各 100nF 去耦。
- 上电日志：
  - 正常：`bme690: BME690 初始化成功: chip_id=0x61 variant_id=0x02`
  - 异常：`bme690: bme69x_init 失败: E_COM_FAIL(I2C?)` → 先查 CSB、上拉、SDO、供电。
- 屏幕：正常显示 `BME690 OK` + T/H/P/Rs；没接/失败显示 `BME690 ERR` 并每 2 秒重试，不会死机。
- 完整调试阶梯、寄存器表、故障排查见 **`..\器件文档\BME690\BME690_使用与调试指南.md`**。

### 命令模板

```bash
export CODEBUDDY_SAFE_DELETE_ENABLED=0
export IDF_PATH="C:/Espressif/frameworks/esp-idf-v5.1.2"
export IDF_TOOLS_PATH="C:/Espressif"
export IDF_PYTHON_ENV_PATH="C:/Espressif/python_env/idf5.1_py3.11_env"
export ESP_ROM_ELF_DIR="C:/Espressif/tools/esp-rom-elfs/20230320"
export PATH="/c/Espressif/tools/xtensa-esp32s3-elf/esp-12.2.0_20230208/xtensa-esp32s3-elf/bin:/c/Espressif/tools/ninja/1.10.2:/c/Espressif/tools/cmake/3.24.0/bin:$PATH"
"/c/Espressif/python_env/idf5.1_py3.11_env/Scripts/python.exe" "C:/Users/zyh/AppData/Local/Temp/idf_run.py" build
```

## 工程约定

- **`sdkconfig.defaults` 是配置权威**；`sdkconfig` 是生成物。改配置 → 改 defaults → 删 sdkconfig → `idf.py set-target esp32s3`。
- `CONFIG_ESPTOOLPY_FLASHMODE_QIO=y` 但 `ESPTOOLPY_FLASHMODE` 字符串仍是 `"dio"` —— **不是 bug**，烧写用 dio、启动后自升 QIO。
- **`FREERTOS_HZ=1000` 是硬需求**：Modbus RTU 帧间静默 ≥3.5 字符 = 4ms@115200；默认 100Hz tick 下 `pdMS_TO_TICKS(4)` 会退化成 10ms，主从同总线时有抢答风险。
- 目标 `esp32s3`；`.vscode/settings.json` 的 `IDF_TARGET` 已修正（历史上曾是 esp32/esp32s2）。
- **显示**：`LCD_ROTATION=3` 为当前横屏 160×128（在方向 1 基础上再转 180°，MADCTL 相应调整）；方向不对改 1。窗口偏移当前 (0,0)。静态画面只刷一帧（1 MHz 下全屏约 0.49 s，反复重刷会闪）。

## 与上位机的联调契约

串口 **115200 / 8N1**，从站 ID **1**，轮询 500ms，超时 800ms。Modbus 4x 保持寄存器：

| 地址 | 内容 | 类型 |
|---|---|---|
| 40001 | TVOC (ppb) | u16 |
| 40002 | eCO2 (ppm) | u16 |
| 40003–40004 | 温度 | float ABCD |
| 40005–40006 | 湿度 | float ABCD |
| 40007 | 电池电压（mV，上位机 ×0.001） | u16 |

⚠️ **TVOC/eCO2 不是 BME690 寄存器直读值**，需跑 BSEC 才能得到；不跑 BSEC 就得改这张表（详见上级 `MEMORY.md` 第 4.2 节）。
