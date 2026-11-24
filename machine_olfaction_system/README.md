# machine_olfaction_system —— ESP32-S3 机器嗅觉固件

基于 **ESP-IDF v5.1.2** 的电子鼻固件工程。目标板：**ESP32-S3-WROOM-1 N16R8**（16 MB Flash + 8 MB Octal PSRAM），调试串口 **COM7 @115200**。

> 全项目背景、选型、采购、硬件评审和调试计划见上级目录的 `MEMORY.md`。

## 当前状态

| 项 | 状态 |
|---|---|
| 编译 / 烧录 | ✅ 通过（2026-11-24，`machine_olfaction_system.bin` 841,664 B，APP 分区剩余约 80%） |
| 显示屏 | ✅ **1.54 寸 ST7789，240×240，RGB565，SPI 40 MHz，默认 0°** |
| UI | ✅ `lvgl_ui` 新导出的 **原生 240×240** 界面：首页 / 检测 / 结果 / 菜单 / 传感器 / 网络 / 蓝牙 / 声音 / 显示 / 系统（昼夜主题）；首页、检测、结果带星系吸收动效；顶部 Wi‑Fi/蓝牙图标随开关与连接状态变化；所有动态提示、状态文字和确认框均为中文 |
| 按键 | ✅ **4 键**：左 / 右 / 确认 / 返回，消抖 + 左右连发；GPIO21/38 已停用 |
| 稳定性 | ✅ 启动约 1.5 s；17 页自动巡页无崩溃、无看门狗复位；LVGL 栈 24 KB，页面按需创建 |
| BME690 | ✅ I²C 实机识别（Chip_Id=0x61 / Variant_Id=0x02）并周期采样温湿度、气压、Rs |
| 业务代码 | ⏳ BSEC/推理、Modbus RTU 从站、真实 Wi‑Fi/BLE/音频仍待接入（当前为模拟服务） |

## 目录

```text
main/app_main.c          初始化顺序：设置 → BSP → LCD → LVGL → 服务 → UI
components/bsp/          引脚、SPI2 DMA、复位、LEDC 背光
components/lcd/          ST7789 240×240 驱动、旋转/偏移、RGB565、XRGB8888 抖动
components/buttons/      4 键轮询、20 ms 消抖、左右键按住连发
components/lvgl_port/    LVGL 9.4 移植、1 ms tick、40 行绘制缓冲、24 KB 任务栈
components/app_services/ NVS 设置、BME690 服务、模拟网络/音频/Modbus
components/bme690/       BME690 I²C 驱动
components/ui/           AiDoGUI 240×240 导出代码 + app_ui.c 四键交互 + galaxy_effect.c
partitions.csv           NVS 24K / phy_init 4K / factory 4M / storage ~11.7M / coredump 512K
sdkconfig.defaults       配置权威；sdkconfig 是生成物
build/                   构建产物（gitignore）
```

## 四键操作

所有按键一端接 GPIO、另一端接 GND；GPIO 内部上拉，按下为低。GPIO21 / GPIO38 不再使用，也不会初始化。

| 按键 | GPIO | 作用 |
|---|---|---|
| 左 | GPIO6 | 焦点左移 / 数值减小 |
| 右 | GPIO7 | 焦点右移 / 数值增大 |
| 确认 | GPIO4 | 确认 / 开始检测 / 进入编辑 |
| 返回 | GPIO5 | 返回 / 取消 / 退出编辑 |

页面逻辑：

- **首页**：确认开始检测；返回进入菜单。首页底部左右两个快捷图标已删除，只保留中央星系动效。
- **检测页**：3.2 s 自动出结果；确认跳过，返回取消。
- **结果页**：确认进入菜单，返回回首页。
- **菜单页**：左右循环选择六张卡片，确认进入，返回回首页。
- **传感器页**：只读展示；返回回菜单。
- **网络 / 蓝牙页**：确认切换开关，返回回菜单。
- **声音页**：左右选行；确认进入音量编辑，编辑中左右调节、确认保存、返回取消；提示音和测试播放用确认键执行。
- **显示页**：左右选行；确认切换昼夜主题、旋转方向、进入亮度编辑或息屏；亮度编辑中左右调节、确认保存、返回取消。
- **系统页**：左右选行；确认切换波特率、重启或恢复默认；返回回菜单。
- 页面切换有 360 ms 输入锁；焦点按页面记忆；音量/亮度编辑只有确认才写 NVS，返回会恢复原值。

## 实时数据

- **传感器 / 检测页**：温度、湿度、气压、气阻、响应值每 500 ms 从 BME690 服务刷新。
- **网络页**：Wi‑Fi 状态、SSID、信号强度、IP、开关状态同步刷新。
- **蓝牙页**：广播/连接状态、设备名称同步刷新。
- **声音 / 显示页**：音量、提示音、亮度、旋转角实时同步。
- **系统页**：设备 ID 使用真实 MAC 后 6 位生成，固件版本读取当前应用描述，波特率来自设置。
- **识别结果页**：推理尚未接入，不再伪造“牛肉 / 新鲜 / 92%”，统一显示 `待接入 / 待检测 / --`。

## 顶部无线状态图标

- **关闭**：对应 Wi‑Fi / 蓝牙图标隐藏，只保留电池图标。
- **开启但未连接**：图标显示为琥珀色，右下角为琥珀色状态点。
- **连接成功**：图标变为绿色，右下角出现绿色勾选徽标，并带 220 ms 淡入动画。
- 所有页面每 500 ms 同步一次状态；切换昼夜主题时自动使用对应的高对比配色。
- Wi‑Fi 与 BLE 当前仍为模拟服务：Wi‑Fi 开启约 1.5 s 后进入“已连接”，BLE 开启约 1.2 s 后进入“已连接”。接入真实协议后只需替换 `net_wifi_connected()` / `net_ble_connected()` 的状态来源。

## 显示实现要点

1. **新屏参数**：ST7789，240×240，RGB565（`COLMOD=0x55`），SPI 40 MHz。
2. **有效区偏移**：本批 `YW-T114-V1.0` 是**原生 240×240 窗口，所有方向 `offset=(0,0)`**。加 80 行偏移会把 UI 整体下移，造成顶部噪声和底部裁切。
3. **背光 BLK**：高电平点亮、低电平关闭；LEDC PWM 已按高有效实现。
4. **默认方向**：0°，`MADCTL=0x00`。NVS 里旧屏遗留的 3 会在本次升级时自动迁移为 0。
5. **SPI 必须 DMA**：单行 RGB565 为 480 B，`max_transfer_sz = 240 × 2`。
6. **启动性能**：AiDoGUI 导出代码默认会一次性建 17 屏，已在 `ui.c` 改成只建首页、其余页面按需创建，避免启动约 8 s 导致任务看门狗。
7. **LVGL 栈**：24 KB；绘制缓冲 240×40×4 B；UI 任务优先级 5。
8. **字体/对比度**：Toast、确认框和网络/蓝牙动态状态使用专用中文点阵字体（14/16/20 px，4 bpp）；浅色主题正文、弱文本、强调色均已加深，深色主题辅助文字提高亮度。

## 星系吸收动效

- **实现**：`components/ui/galaxy_effect.c`，46 个真实 LVGL 粒子，不用 240×240 的大 Canvas。
- **3D 视觉**：椭圆吸积盘、两层旋转轨道弧、粒子径向缩放/亮度衰减、近层切向拉伸和闪烁。
- **页面节奏**：
  - 首页：慢速吸收，适合待机；
  - 检测页：明显加速、提高亮度、吸积盘转快；
  - 结果页：降速缓落，形成“分析完成”的稳定感；
  - 菜单和设置页：关闭粒子层，避免干扰文字与焦点。
- **优化**：33 ms 定时器、固定对象池、粒子到核心后回收复用；任务栈和 LVGL 内存不随页面切换持续增长。
- **同步**：进入页面时自动挂到对应 Stage 的最底层，现有圆环、文字、焦点环和按钮保持在粒子之上。
## 构建 / 烧录（Windows）

```bat
cmd /c "set PROCESSOR_ARCHITECTURE=AMD64&& set IDF_TOOLS_PATH=C:\Espressif&& set IDF_PATH=C:\Espressif\frameworks\esp-idf-v5.1.2&& call C:\Espressif\frameworks\esp-idf-v5.1.2\export.bat >nul && C:\Espressif\python_env\idf5.1_py3.11_env\Scripts\python.exe C:\Espressif\frameworks\esp-idf-v5.1.2\tools\idf.py build"
```

```bat
cmd /c "set PROCESSOR_ARCHITECTURE=AMD64&& set IDF_TOOLS_PATH=C:\Espressif&& set IDF_PATH=C:\Espressif\frameworks\esp-idf-v5.1.2&& call C:\Espressif\frameworks\esp-idf-v5.1.2\export.bat >nul && C:\Espressif\python_env\idf5.1_py3.11_env\Scripts\python.exe C:\Espressif\frameworks\esp-idf-v5.1.2\tools\idf.py -p COM7 flash"
```

## 工程约定

- `sdkconfig.defaults` 是配置权威；改配置后重新 `set-target` 或删除 `sdkconfig`。
- `CONFIG_ESPTOOLPY_FLASHMODE_QIO=y`，但实际烧写可用 DIO，启动后启用 QIO，不是错误。
- `FREERTOS_HZ=1000`：为 Modbus RTU 的 3.5 字符静默时间保留毫秒级精度。
- 新增或修改界面中文提示后，必须重新提取字符并用 `lv_font_conv` 生成 14/16/20 px 三套提示字体，否则新增汉字会显示方框。
- 绑定 `lvgl_ui` 时不要把 `.ai-workspace/session.json` 推到 GitHub，已加入根 `.gitignore`。

## 与上位机联调契约

串口 **115200 / 8N1**，从站 ID **1**，轮询 500 ms，超时 800 ms。Modbus 4x 保持寄存器：

| 地址 | 内容 | 类型 |
|---|---|---|
| 40001 | TVOC (ppb) | u16 |
| 40002 | eCO2 (ppm) | u16 |
| 40003–40004 | 温度 | float ABCD |
| 40005–40006 | 湿度 | float ABCD |
| 40007 | 电池电压（mV，上位机 ×0.001） | u16 |

⚠️ **TVOC/eCO2 不是 BME690 寄存器直读值**，需运行 BSEC 或改为自定义气味指数；尚未接入时不得把原始 Rs 冒充 TVOC/eCO2。
