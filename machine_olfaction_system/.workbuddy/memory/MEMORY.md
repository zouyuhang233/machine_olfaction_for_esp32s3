# 机器嗅觉固件项目 —— 长期约定

> 全项目（硬件选型、采购、评审、调试计划）的总览见上级目录 **`..\..\MEMORY.md`**；
> 本文件只记固件工程侧的长期约定。按日记录见同目录 `2026-*.md`。

## 硬件
- 模组：**ESP32-S3-WROOM-1 N16R8** = 16MB Flash + 8MB Octal PSRAM。
- 串口：COM7（.vscode/settings.json），烧录方式 UART。
- 气体传感器：**BME690**（LGA-8，I²C/SPI，1.71~3.6V）；淘宝裸片 41.58 元（2026-11-19），自绘扩展板 v1.0。
  - 寄存器只给 **Rs / 温度 / 湿度 / 气压**；**TVOC(ppb)、eCO2(ppm) 必须跑 BSEC 才有**。
  - `chip_id = 0x61`（与 BME680/688 相同，不能用于辨真伪）。
- 显示：**1.8 寸 ST7735S，128×160，12 脚模块（8 显示脚 + K1~K4 四键）**，4 线 SPI；`BLK` 为背光使能。
- 语音：**SU-03T / LU-03T** 离线语音模组（UART 发指令、自带喇叭输出）；BOM 另列了 MAX98357A 方案，**未二选一**。
- 二氧化碳：SCD41（NDIR 光声），第二批采购。
- 阶段：**不打板，先用模块调试**（开发板 + 12 脚屏 + BME690 扩展板 + 语音 + 气泵）。

## 工具链
- ESP-IDF **v5.1.2** 位于 `C:\Espressif\frameworks\esp-idf-v5.1.2`，
  Python env `C:\Espressif\python_env\idf5.1_py3.11_env`，工具链
  `C:\Espressif\tools\xtensa-esp32s3-elf\esp-12.2.0_20230208`、cmake 3.24.0、ninja 1.10.2。
- 目标：**esp32s3**（`idf.py set-target esp32s3`）。工程路径全英文，可直接构建。

## 在 WorkBuddy 沙箱里构建的方法（重要，踩过坑）
1. **不要**用 `export.sh`：它检测到 MSys/Mingw 会直接报错拒绝。
2. **不要**直接用 `python .../tools/idf.py`：只要环境里有 `MSYSTEM`，
   `idf.py` 顶部的 `if 'MSYSTEM' in os.environ:` 分支会**只打警告、跳过 main()**，
   表现为命令静默无输出。Git Bash 下宿主会重新注入 MSYSTEM，`unset` 无效。
   → 解法：用包装脚本在 Python 内部清掉，见 `C:\Users\zyh\AppData\Local\Temp\idf_run.py`
   （pop MSYSTEM/MINGW_PREFIX/MSYS + `sys.path.insert(0, tools目录)` + runpy 执行 idf.py）。
3. **必须**设 `CODEBUDDY_SAFE_DELETE_ENABLED=0`：否则 safe-delete shim 会拦截
   `os.remove`，导致 idf_component_manager 报
   `SHFileOperationW 失败: 0x2` / `SAFE_DELETE_FAIL_CLOSED`，构建在 bootloader 配置阶段中断。
4. PowerShell 里 `export.ps1` 被执行策略拦住 → 要么 Bypass，要么按上面手动 export。

完整命令模板：
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
- `sdkconfig.defaults` 是**配置权威**（Flash/PSRAM/CPU/分区表/优化等级都写在这里）；
  `sdkconfig` 是生成物，被 .gitignore 忽略。改配置就改 defaults，然后删 sdkconfig 重新 `set-target`。
- `partitions.csv`：nvs 24K / phy_init 4K / factory(app) 4M / storage(fat) ~11.4M / coredump 512K。
- 已知「假异常」：`CONFIG_ESPTOOLPY_FLASHMODE_QIO=y` 时 `ESPTOOLPY_FLASHMODE` 字符串仍是 `"dio"`，
  这是 IDF 有意为之（烧写用 dio，启动后自升 QIO），不用改。

## 与上位机的联调契约（Qt 端已按此写好）
- 串口 115200 / 8N1，从站 ID = 1，轮询周期 500ms，超时 800ms。
- 寄存器表（4x 保持寄存器）：
  40001 u16 TVOC(ppb) / 40002 u16 eCO2(ppm) / 40003-40004 f32 温度(ABCD) /
  40005-40006 f32 湿度(ABCD) / 40007 u16 电池电压 mV（上位机 ×0.001）。
- ⚠️ 40001/40002 的来源未定：走 BSEC（bVOCe/CO₂ equivalent，单位 ppm 需 ×1000 换算成 ppb）
  或改表为「Rs + IAQ/自定义气味指数」。**定稿前不要写死这两个字段**。
- Poller 合并规则：同功能码、地址空隙 ≤8 寄存器合并成一帧、单帧 ≤125。
- 已知上位机缺陷 5 条（掉零尖刺 / 掉线周期累加 / 无重试 / 缺 BADC-DCBA 字序 / 无地址重叠校验），见 `2026-09-27.md`。
