# 基于 ESP32-S3 的机器嗅觉与食物新鲜度检测系统

> 当前版本：2026-11-25 | ESP32-S3 N16R8 | ST7789 240×240 | BME690 | 4 键交互 | SU-03T 提示音

本项目是一套运行在 ESP32-S3 上的机器嗅觉终端：通过 BME690 采集温度、湿度、气压和气体阻值 Rs，在 240×240 屏幕上显示实时数据、菜单设置、历史记录和气体阻值曲线，并通过 SU-03T / LU-03T 语音模块播放中文提示音。

当前重点是 **稳定、可操作的整机固件与硬件联调**。食物新鲜度推理、真实 Wi-Fi/BLE 连接和 Modbus RTU 从站仍属于后续接口，界面不会伪造识别结果或连接成功状态。

---

## 一、当前完成度

| 模块 | 状态 | 说明 |
|---|---|---|
| ESP32-S3 固件 | 已完成 | ESP-IDF 5.1.2，当前固件 891,808 B，已烧录 COM7 |
| ST7789 屏幕 | 已完成 | 240×240、原生窗口 offset=(0,0)、0° 方向 |
| LVGL UI | 已完成 | 原生 240×240、昼夜主题、全中文字体、星系吸收动效 |
| 4 键交互 | 已完成 | 左/右/确认/返回，GPIO6/7/4/5，按下拉低 |
| BME690 | 已完成 | I2C 0x76，每秒一次温度、湿度、气压和 Rs 采样 |
| 实时曲线 | 已完成 | X 轴时间、Y 轴气体阻值 kΩ，自动量程 |
| 历史记录 | 已完成 | NVS 持久化，最多 20 条 |
| SU-03T 提示音 | 固件已完成 | UART1 发送 1–9 号中文播报消息；模块端需烧录匹配行为 |
| 食物新鲜度推理 | 待接入 | 结果页显示“待接入/待检测”，不生成假结论 |
| 真实 Wi-Fi/BLE | 待接入 | 当前只保存开关状态并显示未连接状态 |
| Modbus RTU 从站 | 待接入 | 当前保留参数和状态文本 |

## 二、硬件组成

| 部件 | 型号/规格 | 关键说明 |
|---|---|---|
| 主控 | ESP32-S3 N16R8 | 16 MB Flash、8 MB PSRAM，当前串口 COM7 @ 115200 |
| 屏幕 | ST7789 1.54 寸 240×240 | 8 Pin SPI，RGB565，SPI2 DMA，40 MHz |
| 气体/环境传感器 | BME690 | I2C 0x76，SDA GPIO8，SCL GPIO18 |
| 按键 | 4 个独立轻触键 | 左/右/确认/返回；内部上拉，按下拉低 |
| 语音模块 | SU-03T / LU-03T | 串口控制、自带功放；模块 VCC 5V |
| 喇叭 | 4Ω 或 8Ω | 只接模块 SPK+ / SPK-，不能接 ESP32 GPIO |

### 2.1 完整引脚表

| 功能 | ESP32-S3 引脚 | 对方引脚 | 说明 |
|---|---|---|---|
| 屏幕 SCLK | GPIO12 | SCL/SCLK | SPI2 时钟 |
| 屏幕 MOSI | GPIO11 | SDA/MOSI | SPI2 数据 |
| 屏幕 CS | GPIO10 | CS | 片选 |
| 屏幕 DC | GPIO9 | DC | 数据/命令 |
| 屏幕 RST | GPIO14 | RST | 复位 |
| 屏幕 BL | GPIO13 | BLK | LEDC PWM 背光，高电平点亮 |
| BME690 SDA | GPIO8 | SDI/SDA | I2C 数据，4.7k 上拉 |
| BME690 SCL | GPIO18 | SCK/SCL | I2C 时钟，4.7k 上拉 |
| 左键 | GPIO6 | 按键一端 | 另一端 GND，按下为低 |
| 右键 | GPIO7 | 按键一端 | 另一端 GND，按下为低 |
| 确认键 | GPIO4 | 按键一端 | 另一端 GND，按下为低 |
| 返回键 | GPIO5 | 按键一端 | 另一端 GND，按下为低 |
| SU-03T RX | GPIO15 / UART1_TX | B6 / UART1_RXD | ESP32 发消息给语音模块 |
| SU-03T TX | GPIO16 / UART1_RX | B7 / UART1_TXD | 语音模块回传消息给 ESP32 |
| 共地 | GND | 模块 GND | ESP32、屏幕、BME690、语音模块必须共地 |

### 2.2 电源与安全

- 屏幕只接 3.3V，不要接 5V。
- BME690 使用 3.3V I2C 逻辑；扩展板 CSB 拉高、SDO 接地时地址为 0x76。
- SU-03T 模块 VCC 接 5V，电流至少 550 mA，建议 5V/1A；模块 3V3 是输出，不能当输入。
- 喇叭只接模块 18 SPK+、17 SPK-；SPK± 是差分输出，不能把任意一端接 GND。
- GPIO17/18 不能拿来接语音 UART：GPIO18 已被 BME690 I2C SCL 占用。

## 三、快速开始

### 3.1 构建

```bat
cd /d C:\Users\zyh\Desktop\machine_olfaction_for_esp32s3\machine_olfaction_system
set IDF_TOOLS_PATH=C:\Espressif
set IDF_PATH=C:\Espressif\frameworks\esp-idf-v5.1.2
call C:\Espressif\frameworks\esp-idf-v5.1.2\export.bat
C:\Espressif\python_env\idf5.1_py3.11_env\Scripts\python.exe C:\Espressif\frameworks\esp-idf-v5.1.2\tools\idf.py build
```

### 3.2 烧录与监视

```bat
C:\Espressif\python_env\idf5.1_py3.11_env\Scripts\python.exe C:\Espressif\frameworks\esp-idf-v5.1.2\tools\idf.py -p COM7 flash
C:\Espressif\python_env\idf5.1_py3.11_env\Scripts\python.exe C:\Espressif\frameworks\esp-idf-v5.1.2\tools\idf.py -p COM7 monitor
```

退出监视按 `Ctrl+]`。详细步骤见 [docs/QUICKSTART.md](docs/QUICKSTART.md)。

## 四、4 键操作总览

| 按键 | GPIO | 通用作用 |
|---|---|---|
| 左 | GPIO6 | 焦点左移 / 数值减小 / 曲线页返回 |
| 右 | GPIO7 | 焦点右移 / 数值增加 / 首页进入实时监测 |
| 确认 | GPIO4 | 进入 / 开始检测 / 保存 / 切换开关 |
| 返回 | GPIO5 | 退出编辑 / 返回上一级 / 首页进入菜单 |

- 首页：确认开始检测；右键进入实时监测；返回键进入菜单。
- 菜单包含 7 张卡片：传感器、网络、蓝牙、声音、显示、系统、历史。
- 设置页左右选择或调节，确认保存，返回取消。
- 历史记录左右翻页，确认或返回关闭。

完整页面矩阵见 [docs/UI_OPERATION.md](docs/UI_OPERATION.md)。

## 五、SU-03T 中文提示音

ESP32 使用 UART1、9600 8N1 发送官方串口输入帧：

```text
AA 55 消息号 55 AA
```

| 消息号 | 触发时机 | 建议播报 |
|---:|---|---|
| 1 | 开始检测 | 开始检测 |
| 2 | 检测完成并保存成功 | 检测完成 |
| 3 | 检测取消 | 检测已取消 |
| 4 | 传感器数据无效 | 传感器连接异常，请检查 |
| 5 | 设置保存成功 | 设置已保存 |
| 6 | 功能开启 | 功能已开启 |
| 7 | 功能关闭 | 功能已关闭 |
| 8 | 测试播放 | 提示音正常 |
| 9 | 历史保存失败 | 保存失败，请重试 |

> 重要：只接好喇叭还不够。出厂固件没有本项目的 1–9 号串口输入行为，必须在智能公元平台生成并烧录匹配固件。详细步骤见 [提示音串口协议](器件文档/SU-03T语音模块/提示音串口协议.md)。

## 六、仓库结构

```text
machine_olfaction_for_esp32s3/
├─ machine_olfaction_system/      ESP-IDF 固件工程
│  ├─ components/app_services/   NVS、BME690 服务、历史记录、SU-03T UART
│  ├─ components/ui/             AiDoGUI 页面、4 键交互、星系动效、中文字库
│  ├─ components/bme690/         BME690 I2C 与 Bosch 驱动封装
│  ├─ components/lcd/            ST7789 驱动
│  ├─ components/buttons/        4 键扫描与消抖
│  └─ main/app_main.c            初始化顺序与主入口
├─ lvgl_ui/                      可编辑的 LVGL 设计工程
├─ 器件文档/                     屏幕、BME690、SU-03T 和接线资料
├─ 硬件制作/                     扩展底板、BME690 扩展板、评审与上电检查
├─ 选题/                         开题报告、BOM 与选型说明
├─ docs/                         快速上手、UI、架构、测试与排障
└─ MEMORY.md                     项目决策、历史记录和长期上下文
```

## 七、文档导航

| 文档 | 内容 |
|---|---|
| [docs/README.md](docs/README.md) | 全量文档索引与阅读顺序 |
| [docs/QUICKSTART.md](docs/QUICKSTART.md) | 环境、构建、烧录、串口监视和上电检查 |
| [docs/HARDWARE.md](docs/HARDWARE.md) | 接线、电源、上电检查和验收表 |
| [docs/UI_OPERATION.md](docs/UI_OPERATION.md) | 4 键页面矩阵、编辑流程和提示音节点 |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | 启动顺序、组件职责、任务并发和数据流 |
| [docs/TESTING_AND_TROUBLESHOOTING.md](docs/TESTING_AND_TROUBLESHOOTING.md) | 实机验证记录、自检清单和故障排查 |
| [machine_olfaction_system/README.md](machine_olfaction_system/README.md) | 固件详细实现、引脚、显示和 Modbus 预留表 |
| [MEMORY.md](MEMORY.md) | 项目决策、历史、踩坑和待办 |
| [器件文档/README.md](器件文档/README.md) | 器件资料索引 |
| [器件文档/语音模块与喇叭/README.md](器件文档/语音模块与喇叭/README.md) | 喇叭、5V 供电、UART 实物接线 |
| [器件文档/SU-03T语音模块/提示音串口协议.md](器件文档/SU-03T语音模块/提示音串口协议.md) | 1–9 号消息和模块端配置 |
| [硬件制作/README.md](硬件制作/README.md) | 扩展底板与 BME690 扩展板总览 |
| [选题/README.md](选题/README.md) | 开题报告、BOM 和选型差异 |

## 八、已验证与未完成边界

### 已验证

- ST7789 240×240 正常点亮，旋转、偏移和背光控制正确。
- BME690 实机识别，Chip_Id=0x61、Variant_Id=0x02，温湿度和气压正常。
- 4 键导航、长按连发、焦点保持、编辑确认/取消、息屏唤醒正常。
- 菜单 7 卡片、历史记录 NVS、实时曲线和自动量程正常。
- SU-03T UART1 初始化成功，提示音消息队列和 9 类事件发送路径已接入。
- 2026-11-25 固件构建、COM7 烧录和启动日志验证通过。

### 未完成

- 食物新鲜度模型与训练数据、推理输出。
- 真实 Wi-Fi 配网/连接和 BLE 服务。
- Modbus RTU 从站寄存器读写。
- SU-03T 模块端 1–9 号串口输入行为的实际烧录与逐条试听。
- TVOC/eCO2 的 BSEC 或自定义气味指数。

## 九、开发约定

1. UI 不直接操作 UART、I2C、Wi-Fi 或 BLE，统一通过 `components/app_services`。
2. 耗时操作不能放在 LVGL 线程；提示音使用单元素队列和独立发送任务。
3. 不在界面伪造传感器数据、推理结果、Wi-Fi/BLE 连接成功或 Modbus 状态。
4. 修改用户可见中文后，必须同步检查 `ui_font_prompt_cn_12/14/16/20` 字库字符覆盖。
5. 文档、引脚表和接线说明必须与当前代码一起更新。

## 十、说明

本仓库包含课程/竞赛项目的硬件资料、固件和文档。第三方数据手册、厂商工具和固件归各自权利人所有；仓库内引用仅用于学习、调试与设备集成。
