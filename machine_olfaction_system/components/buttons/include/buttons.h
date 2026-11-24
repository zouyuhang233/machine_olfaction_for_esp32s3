/*
 * buttons.h -- 4 个按键轮询 + 消抖 + 左右连发
 *
 * 按键：左 / 右 / 确认 / 返回。按下拉低到 GND，GPIO 内部上拉。
 * buttons_poll_event() 每次返回一个"新按下"事件。
 */
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BTN_LEFT = 0,
    BTN_RIGHT,
    BTN_OK,
    BTN_BACK,
    BTN_COUNT,
} button_id_t;

typedef struct {
    button_id_t id;
    bool repeat;    /* true 表示按住左/右键产生的连发事件 */
} button_event_t;

/** 配置 GPIO 为上拉输入。 */
void buttons_init(void);

/** 轮询一次；返回带连发标记的完整事件。 */
bool buttons_poll_event(button_event_t *out);

/** 兼容接口：只返回按键 ID。 */
bool buttons_poll(button_id_t *out);

/** 取消左右键当前的按住连发状态（用于息屏唤醒后避免误滚动）。 */
void buttons_cancel_repeat(void);

#ifdef __cplusplus
}
#endif
