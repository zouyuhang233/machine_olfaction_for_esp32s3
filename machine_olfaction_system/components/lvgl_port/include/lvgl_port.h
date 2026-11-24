/*
 * lvgl_port.h -- LVGL 显示移植层
 *
 * 依赖 lcd 组件已经完成初始化和旋转（lcd_init + lcd_set_rotation）。
 * 只负责：LVGL 初始化、flush 回调、1 ms tick、LVGL 任务。
 */
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 初始化 LVGL、显示驱动和 tick（此时 LVGL 任务还未启动）。 */
esp_err_t lvgl_port_init(void);

/** 启动 LVGL 任务。请在创建完初始界面后调用，避免并发访问。 */
esp_err_t lvgl_port_start(void);

#ifdef __cplusplus
}
#endif
