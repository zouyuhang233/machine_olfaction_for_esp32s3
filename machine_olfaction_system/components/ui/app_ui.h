#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 初始化 AiDoGUI 新界面、6 键输入和业务数据绑定。 */
esp_err_t app_ui_init(void);

/** 关闭背光；下一次按键只负责唤醒，不触发页面动作。 */
void app_ui_screen_off(void);

#ifdef __cplusplus
}
#endif

