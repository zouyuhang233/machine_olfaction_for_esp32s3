# lvgl_ui —— AiDoGUI Designer 新界面源工程

- 工程文件：`lvgl_ui.aid`
- 画布：**240 × 240**，LVGL **9.4.0**
- 页面数：17
  - 首页、检测、结果
  - 菜单（黑夜 / 白天）
  - 传感器、网络、蓝牙、声音、显示、系统（各含黑夜 / 白天）
- 导出代码：`ui/`
- 原始字体/图片资源：`assets/`
- 全页面截图：`previews/240x240/`，总览见 `contact_sheet.png`

## 固件同步规则

1. 在 AiDoGUI Designer 修改并导出 C 代码到 `ui/`。
2. 将 `ui/` 同步到 `machine_olfaction_system/components/ui/`。
3. 保留固件专用的 `app_ui.c`、`app_ui.h` 与 `CMakeLists.txt`，不要用导出文件覆盖。
4. 检查导出 `ui.c` 是否又变成“启动创建全部页面”；固件必须只建首页，其余页面按需创建，否则会启动卡死并触发看门狗。

## 注意

`.ai-workspace/` 是本机 AiDoGUI 会话目录，含有临时令牌，已在根 `.gitignore` 排除，**不要提交**。
- 当前固件的 4 键操作、页面矩阵、提示音触发点和演示流程见 `..\docs\UI_OPERATION.md`。
