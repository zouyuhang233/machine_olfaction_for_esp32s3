#ifndef AIDOGUI_FLOW_H
#define AIDOGUI_FLOW_H
#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>
typedef struct { int type; double number; char text[1025]; } aido_flow_value_t;
typedef struct aido_flow_state aido_flow_state_t;
typedef int (*aido_flow_step_fn)(aido_flow_state_t *);
typedef struct { aido_flow_step_fn run; int reentry; bool project_lifetime; const char * id; } aido_flow_definition_t;
typedef struct aido_flow_animation aido_flow_animation_t;
struct aido_flow_state {
    unsigned pc;
    uint32_t generation;
    bool failed;
    bool finished;
    const char * node;
    void * context;
    lv_obj_t * source;
    lv_obj_t * screen;
    lv_event_t * native_event;
    const aido_flow_definition_t * definition;
    aido_flow_value_t snapshot[8];
    uint32_t wake;
    bool delayed;
    lv_obj_t * waiting_screen;
    lv_obj_t * watched_screen;
    unsigned group;
    unsigned waiting_group;
    aido_flow_animation_t * animations;
    struct aido_flow_state * next;
    struct aido_flow_queue * first;
    struct aido_flow_queue * last;
    unsigned queued;
};
/* 在对象创建阶段注册；不能在该对象事件回调期间首次注册。 */
void aido_flow_observe(lv_obj_t *);
void aido_flow_init(void);
void aido_flow_destroy(void);
void aido_flow_trigger(const aido_flow_definition_t *, void *, lv_obj_t *, lv_event_t *);
void aido_flow_error(aido_flow_state_t *, const char *);
int aido_flow_delay(aido_flow_state_t *, aido_flow_value_t);
void aido_flow_wait_screen(aido_flow_state_t *, lv_obj_t *);
bool aido_flow_is_waiting(aido_flow_state_t *);
void aido_flow_navigation_started(lv_obj_t *);
void aido_flow_context_destroyed(void *);
void aido_flow_begin_animation(aido_flow_state_t *, bool);
void aido_flow_track_animation(aido_flow_state_t *, lv_anim_t *);
void aido_flow_watch_animation(aido_flow_state_t *, lv_anim_t *);
void aido_flow_animation_interrupted(void *, int);
void aido_flow_animation_deleted(void *);
void aido_flow_animation_completed(lv_anim_t *);
aido_flow_value_t aido_flow_number(aido_flow_state_t *, int, double);
aido_flow_value_t aido_flow_string(aido_flow_state_t *, const char *);
aido_flow_value_t aido_flow_boolean(bool);
bool aido_flow_bool(aido_flow_state_t *, aido_flow_value_t);
double aido_flow_num(aido_flow_state_t *, aido_flow_value_t);
const char * aido_flow_text(aido_flow_state_t *, const aido_flow_value_t *);
aido_flow_value_t aido_flow_binary(aido_flow_state_t *, const char *, aido_flow_value_t, aido_flow_value_t);
aido_flow_value_t aido_flow_unary(aido_flow_state_t *, bool, aido_flow_value_t);
aido_flow_value_t aido_flow_convert(aido_flow_state_t *, int, aido_flow_value_t);
aido_flow_value_t aido_flow_concat(aido_flow_state_t *, aido_flow_value_t, aido_flow_value_t);
aido_flow_value_t aido_flow_variable(aido_flow_state_t *, const char *, aido_flow_value_t);
void aido_flow_assign(aido_flow_state_t *, const char *, aido_flow_value_t, aido_flow_value_t);
aido_flow_value_t aido_flow_event(aido_flow_state_t *, const char *);
aido_flow_value_t aido_flow_widget(aido_flow_state_t *, lv_obj_t *, const char *, const char *);
#endif
