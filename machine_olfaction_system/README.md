# machine_olfaction_system —— ESP32-S3 机器嗅觉固件

基于 **ESP-IDF v5.1.2** 的电子鼻固件工程。目标板：**ESP32-S3-WROOM-1 N16R8**（16 MB Flash + 8 MB Octal PSRAM），调试串口 **COM7 @115200**。

> 全项目背景、选型、采购、硬件评审和调试计划见上级目录的 `MEMORY.md`。

## 当前状态

| 项 | 状态 |
|---|---|
| 编译 / 烧录 | ✅ 通过（2026-11-24，`machine_olfaction_system.bin` 835,056 B，APP 分区剩余约 80%） |
| 显示屏 | ✅ **1.54 寸 ST7789，240×240，RGB565，SPI 40 MHz，默认 0°** |
| UI | ✅ `lvgl_ui` 新导出的 **原生 240×240** 界面：首页 / 检测 / 结果 / 菜单 / 传感器 / 网络 / 蓝牙 / 声音 / 显示 / 系统（昼夜主题）；首页、检测、结果带星系吸收动效 |
| 按键 | ✅ **6 键**：UP / DOWN / LEFT / RIGHT / OK / BACK，消抖 + 方向键连发 |
| 稳定性 | ✅ 启动约 1.5 s；17 页自动巡页无崩溃、无看门狗复位；LVGL 栈 24 KB，页面按需创建 |
| BME690 | ✅ I²C 实机识别（Chip_Id=0x61 / Variant_Id=0x02）并周期采样温湿度、气压、Rs |
| 业务代码 | ⏳ BSEC/推理、Modbus RTU 从站、真实 Wi‑Fi/BLE/音频仍待接入（当前为模拟服务） |

## 目录

```text
main/app_main.c          初始化顺序：设置 → BSP → LCD → LVGL → 服务 → UI
components/bsp/          引脚、SPI2 DMA、复位、LEDC 背光
components/lcd/          ST7789 240×240 驱动、旋转/偏移、RGB565、XRGB8888 抖动
components/buttons/      6 键轮询、20 ms 消抖、按住连发
components/lvgl_port/    LVGL 9.4 移植、1 ms tick、40 行绘制缓冲、24 KB 任务栈
components/app_services/ NVS 设置、BME690 服务、模拟网络/音频/Modbus
components/bme690/       BME690 I²C 驱动
components/ui/           AiDoGUI 240×240 导出代码 + app_ui.c 六键交互 + galaxy_effect.c
partitions.csv           NVS 24K / phy_init 4K / factory 4M / storage ~11.7M / coredump 512K
sdkconfig.defaults       配置权威；sdkconfig 是生成物
build/                   构建产物（gitignore）
```

## 六键操作

所有按键一端接 GPIO、另一端接 GND；GPIO 内部上拉，按下为低。

| 按键 | GPIO | 通用作用 |
|---|---|---|
| UP | GPIO4 | 上移焦点 / 增加数值 |
| DOWN | GPIO5 | 下移焦点 / 减少数值 |
| LEFT | GPIO6 | 左移焦点 / 返回 / 减少数值 |
| RIGHT | GPIO7 | 右移焦点 / 进入 / 增加数值 |
| OK | GPIO21 | 确认 / 开始检测 / 进入编辑 |
| BACK | GPIO38 | 返回 / 取消 / 退出编辑 |

页面逻辑：

- **首页**：OK 开始检测；BACK 进入菜单。焦点环围绕中央识别圆环。
- **检测页**：3.2 s 自动出结果；OK/RIGHT 跳过，BACK/LEFT 取消。
- **结果页**：OK/RIGHT 进入菜单，BACK/LEFT 回首页。
- **菜单页**：2×3 六卡片；方向键移动焦点并支持按住连发，OK 进入，BACK 回首页。
- **传感器页**：只读展示；BACK/LEFT 回菜单。
- **网络 / 蓝牙页**：OK 切换开关；LEFT 关闭，RIGHT 打开；BACK 返回。
- **声音页**：UP/DOWN 选行；音量行 LEFT/RIGHT 连续调节；OK 进入/保存音量编辑；提示音行 LEFT/RIGHT/OK 切换；测试播放行 RIGHT/OK 播放测试音。
- **显示页**：第 1 行 LEFT/RIGHT/OK 切换昼夜主题；第 2 行调整旋转方向；第 3 行直接调亮度，OK 进入连续调节；第 4 行 OK/方向键息屏，任意键唤醒。
- **系统页**：第 1 行切换 Modbus 波特率；重启 / 恢复默认均弹出 CANCEL / OK 确认框。
- 页面切换有 360 ms 输入锁；焦点按页面记忆；音量/亮度编辑结束才批量写 NVS，避免连发擦写。

## 显示实现要点

1. **新屏参数**：ST7789，240×240，RGB565（`COLMOD=0x55`），SPI 40 MHz。
2. **有效区偏移**：本批 `YW-T114-V1.0` 是**原生 240×240 窗口，所有方向 `offset=(0,0)`**。加 80 行偏移会把 UI 整体下移，造成顶部噪声和底部裁切。
3. **背光 BLK**：高电平点亮、低电平关闭；LEDC PWM 已按高有效实现。
4. **默认方向**：0°，`MADCTL=0x00`。NVS 里旧屏遗留的 3 会在本次升级时自动迁移为 0。
5. **SPI 必须 DMA**：单行 RGB565 为 480 B，`max_transfer_sz = 240 × 2`。
6. **启动性能**：AiDoGUI 导出代码默认会一次性建 17 屏，已在 `ui.c` 改成只建首页、其余页面按需创建，避免启动约 8 s 导致任务看门狗。
7. **LVGL 栈**：24 KB；绘制缓冲 240×40×4 B；UI 任务优先级 5。
8. **字体/对比度**：文本标签按字体高度自适应；浅色主题正文、弱文本、强调色均已加深。

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
