# 快速上手：接线、构建、烧录与监视

本文面向第一次拿到本项目硬件和源码的人。按顺序完成即可得到可操作的 4 键界面。

## 一、准备清单

- ESP32-S3 N16R8 开发板。
- ST7789 1.54 寸 240×240 8 Pin SPI 屏。
- BME690 传感器或已焊好的 BME690 扩展板。
- 4 个独立轻触按键。
- SU-03T / LU-03T 语音模块、4Ω 或 8Ω 喇叭、5V/1A 电源。
- USB 数据线和电脑。
- ESP-IDF 5.1.2、Python 3.11、CMake、Ninja 和串口驱动。

## 二、接线检查

| 功能 | 接线 |
|---|---|
| 屏幕 | SCLK=12，MOSI=11，CS=10，DC=9，RST=14，BL=13，VCC=3V3，GND=GND |
| BME690 | SDA=8，SCL=18，VDDIO/VDD=3V3，GND=GND，地址 0x76 |
| 左键 | GPIO6 与 GND |
| 右键 | GPIO7 与 GND |
| 确认键 | GPIO4 与 GND |
| 返回键 | GPIO5 与 GND |
| 语音模块 UART | ESP32 GPIO15(TX) → 模块 B6(RXD)，GPIO16(RX) ← 模块 B7(TXD)，GND 共地 |
| 语音模块电源 | 模块 VCC=5V，建议 5V/1A；模块 3V3 不接 |
| 喇叭 | 红线/正极接模块 18 SPK+，黑线/负极接模块 17 SPK- |

注意：喇叭不能直接接 ESP32 GPIO、3.3V 或 5V；SPK+ 和 SPK- 都不能接 GND。

## 三、上电前检查

1. 用万用表确认 3.3V 和 5V 没有短路。
2. 确认屏幕没有接到 5V。
3. 确认 BME690 的 SDA/SCL 没有接反，CSB 为高、SDO 为低时地址是 0x76。
4. 确认 4 个按键另一端都接 GND，没有把 3.3V 接到按键。
5. 确认语音模块和 ESP32 共地，模块 VCC 由独立 5V 供电。
6. 第一次调试时先不接语音模块 UART，只验证屏幕、BME690 和按键。

## 四、构建固件

在 Windows 上使用项目已验证的 ESP-IDF 5.1.2 环境：

```bat
cd /d C:\Users\zyh\Desktop\machine_olfaction_for_esp32s3\machine_olfaction_system
set PROCESSOR_ARCHITECTURE=AMD64
set IDF_TOOLS_PATH=C:\Espressif
set IDF_PATH=C:\Espressif\frameworks\esp-idf-v5.1.2
call C:\Espressif\frameworks\esp-idf-v5.1.2\export.bat
C:\Espressif\python_env\idf5.1_py3.11_env\Scripts\python.exe C:\Espressif\frameworks\esp-idf-v5.1.2\tools\idf.py build
```

如果已经打开 ESP-IDF PowerShell 或 VS Code 的 ESP-IDF 终端，直接在 `machine_olfaction_system` 目录运行：

```powershell
idf.py build
```

构建成功后固件位于：

```text
machine_olfaction_system\build\machine_olfaction_system.bin
```

## 五、烧录与监视

确认设备管理器里的串口，例如当前实机使用 COM7：

```bat
C:\Espressif\python_env\idf5.1_py3.11_env\Scripts\python.exe C:\Espressif\frameworks\esp-idf-v5.1.2\tools\idf.py -p COM7 flash
C:\Espressif\python_env\idf5.1_py3.11_env\Scripts\python.exe C:\Espressif\frameworks\esp-idf-v5.1.2\tools\idf.py -p COM7 monitor
```

监视器退出键是 `Ctrl+]`。

### 正常启动日志应包含

```text
lcd: ST7789 就绪: 240x240, RGB565, offset=(0,0)
services: SU-03T prompts ready on UART1
bme690: Chip_Id=0x61 Variant_Id=0x02 (BME690)
services: SU-03T UART1 ready: TX GPIO15, RX GPIO16, 9600 8N1
app_ui: AiDoGUI UI + 4 键适配已启动
```

## 六、首次功能验证

1. 上电后等待约 1.5 秒，确认首页和星系动效出现。
2. 按右键进入实时监测，观察气体阻值曲线每秒增加一个点。
3. 按左键或返回键回首页。
4. 按返回键进入菜单，左右切换 7 张卡片，确认可以进入各设置页。
5. 进入声音页，确认测试播放会发送 8 号提示音消息。
6. 回到首页按确认开始检测，等待约 3.2 秒进入结果页并保存历史记录。
7. 在菜单进入历史，确认新记录可翻页查看。

## 七、SU-03T 模块端准备

ESP32 端已经会发送提示音帧，但模块端必须存在对应行为：

1. 在智能公元平台启用 UART1，B6 为 RX，B7 为 TX。
2. 创建 1–9 号串口输入行为，全部不添加参数。
3. 每个行为添加“播放语音”控制，填写对应中文播报。
4. 生成并烧录模块固件，串口固定 9600 8N1。
5. 用串口工具发送 `AA 55 08 55 AA`，应播放“提示音正常”。

消息号表与平台配置见 [SU-03T 提示音串口协议](../器件文档/SU-03T语音模块/提示音串口协议.md)。

## 八、常见首次失败

| 现象 | 优先检查 |
|---|---|
| 白屏 | 屏幕 3.3V、BL、SPI 引脚、offset 和旋转 |
| BME690 未找到 | SDA/SCL、供电、CSB、SDO、地址 0x76 |
| 按键无反应 | GPIO6/7/4/5 是否按下拉低，另一端是否接 GND |
| 没有声音 | 模块 5V、共地、SPK±、UART 15/16 交叉接线、模块固件消息号 |
| 烧录失败 | COM 口被监视器占用、驱动、USB 线、开发板进入下载模式 |

更多排查见 [测试与排障](TESTING_AND_TROUBLESHOOTING.md)。
