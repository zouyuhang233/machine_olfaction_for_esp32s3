# 软件架构与并发模型

本文说明当前固件的启动顺序、组件职责、任务、数据流和扩展边界。

## 一、启动顺序

```text
app_main
  → bsp_init
  → settings_store_init
  → lcd_init
  → lcd_set_rotation
  → lvgl_port_init
  → services_init
  → app_ui_init
  → lvgl_port_start
  → backlight on
```

`services_init()` 会在 UI 启动前完成：

- NVS 历史记录加载。
- BME690 采集任务创建。
- SU-03T UART1 初始化。
- 提示音队列和发送任务创建。

## 二、组件职责

| 组件 | 职责 |
|---|---|
| `components/bsp` | GPIO、SPI2 DMA、LCD 复位、LEDC 背光 |
| `components/lcd` | ST7789 初始化、旋转、偏移、RGB565 刷屏 |
| `components/lvgl_port` | LVGL 9.4 显示缓冲、1 ms tick、LVGL 任务 |
| `components/buttons` | 4 键轮询、12 ms 消抖、左右长按连发 |
| `components/bme690` | I2C、Bosch 驱动、身份校验、温湿度和 Rs 采样 |
| `components/app_services` | 业务 API、设置持久化、采集任务、历史、SU-03T UART |
| `components/ui` | AiDoGUI 导出页面、app_ui 状态机、焦点、Toast、星系动效 |
| `main` | 组装初始化和主循环 |

UI 不直接操作 UART、I2C、Wi-Fi 或 BLE，所有访问都通过 `services.h`。

## 三、任务与并发

| 执行体 | 触发方式 | 工作内容 |
|---|---|---|
| LVGL 任务 | `lvgl_port_start()` | 页面渲染、动画、Toast、图表刷新 |
| UI 输入定时器 | LVGL 定时器 8 ms | 轮询按键、处理页面状态、焦点更新 |
| 数据刷新定时器 | LVGL 定时器 500 ms | 调 `services_update()` 并刷新当前页面数值 |
| BME690 采集任务 | FreeRTOS 任务 | 每 1 s forced 测量，更新互斥保护的缓存 |
| SU-03T 发送任务 | FreeRTOS 任务 | 从单元素队列取消息号，写 UART1，80 ms 间隔 |

关键原则：

- LVGL 线程绝不等待串口、I2C 或网络。
- BME690 缓存由互斥锁保护，UI 只取快照。
- 提示音队列长度只有 1，密集事件只保留最新一条，避免积压。
- 屏幕切换时销毁离页对象，避免 17 页同时驻留导致内存和看门狗问题。

## 四、数据流

### 4.1 传感器

```text
BME690 I2C
  → bme690 组件
  → sensor_cache_t
  → sensor_get 快照
  → 首页/传感器页/监测页/历史记录
```

`sensor_data_t` 中：

- `temp` 单位 °C。
- `humi` 单位 %RH。
- `press` 单位 hPa。
- `rs` 单位 kΩ。
- `tvoc`、`eco2` 在未接 BSEC 时为 NAN，UI 显示 `--`，不能把 Rs 冒充 TVOC。

### 4.2 历史记录

历史记录使用 NVS blob：

```text
history_blob_t
  magic = 0x48495354
  head/count/next_sequence
  records[20]
```

检测完成时调用 `history_capture_current()`，保存当前快照。记录满 20 条后从最旧位置循环覆盖。

### 4.3 提示音

```text
UI 事件
  → audio_prompt_play(AUDIO_PROMPT_xxx)
  → 单元素队列
  → su03t_audio 任务
  → UART1 9600 8N1
  → AA 55 消息号 55 AA
  → SU-03T 中文播报
```

提示音总开关关闭时，除测试消息外都不会入队。关闭提示音前会先播报“功能已关闭”。

## 五、主要服务接口

| 类别 | 接口 |
|---|---|
| 初始化 | `services_init()`、`services_update()` |
| 传感器 | `sensor_get()`、`sensor_status_text()` |
| 历史 | `history_capture_current()`、`history_count()`、`history_get()` |
| Wi-Fi | `net_wifi_enabled()`、`net_wifi_set_enabled()`、`net_wifi_connected()` |
| BLE | `net_ble_enabled()`、`net_ble_set_enabled()`、`net_ble_connected()` |
| 音频 | `audio_prompt_play()`、`audio_play_test()`、音量/提示音开关接口 |
| Modbus | 地址、波特率和状态文本接口 |

## 六、UI 状态机

`app_ui.c` 维护页面 ID、焦点索引、编辑模式、确认框、Toast、历史覆盖层和输入锁：

- `SCR_HOME`、`SCR_MONITOR`、`SCR_DETECTION`、`SCR_RESULT`。
- 深色/浅色菜单和 6 个设置子页。
- `EDIT_NONE`、`EDIT_VOLUME`、`EDIT_BRIGHTNESS`。
- `CONFIRM_NONE`、`CONFIRM_REBOOT`、`CONFIRM_RESET`。

每次切页都会初始化目标页、同步主题、刷新数据、恢复焦点并设置约 180 ms 输入锁。

## 七、显示与内存策略

- ST7789 使用 SPI2 DMA，RGB565，40 MHz。
- LVGL 使用 40 行绘制缓冲，颜色格式为 XRGB8888 内部格式。
- LVGL 任务栈 24 KB。
- 页面按需创建和销毁，不一次创建全部 17 屏。
- 大图标、字体和图片资源按需加载，避免内部 RAM 被占满。
- 星系动效只挂在首页、检测页和结果页；菜单和设置页关闭粒子层。

## 八、设置持久化

`settings_store.c` 使用 NVS 命名空间 `appcfg` 保存：

- Wi-Fi/BLE 开关。
- 音量和提示音开关。
- 亮度、旋转。
- Modbus 地址和波特率。
- 服务器地址预留。

当前 `disp_ver=2` 会把旧屏遗留的旋转值迁移为 0°。

## 九、预留扩展点

### 9.1 食物新鲜度推理

结果页已经有食物名称、新鲜度和置信度控件，但目前主动显示“待接入/待检测/--”。接入推理时应：

1. 在 `app_services` 中增加推理服务，不让 UI 直接调用模型。
2. 明确定义输入特征窗口、采样频率和气体阻值基线。
3. 返回置信度和模型版本，不能只返回一个类别字符串。
4. 模型失败时仍显示“待检测”，不能用默认类别冒充成功。

### 9.2 真实 Wi-Fi/BLE

当前 `net_wifi_connected()` 和 `net_ble_connected()` 始终返回 false。接入真实协议时，顶部图标只能由真实连接事件驱动变绿。

### 9.3 Modbus RTU 从站

寄存器表已在固件 README 中预留，当前只保存地址和波特率，不处理请求。接入后可把温度、湿度、气压和 Rs 映射到 Holding Register。

### 9.4 SU-03T 模块固件

ESP32 端消息发送已完成；模块端还需要 1–9 号串口输入行为和中文播报。详见 [提示音串口协议](../器件文档/SU-03T语音模块/提示音串口协议.md)。
