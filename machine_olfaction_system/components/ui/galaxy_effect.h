#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GALAXY_MODE_OFF = 0,
    GALAXY_MODE_HOME,
    GALAXY_MODE_DETECTION,
    GALAXY_MODE_RESULT,
} galaxy_mode_t;

void galaxy_effect_init(void);
void galaxy_effect_attach(lv_obj_t *stage, galaxy_mode_t mode, int center_x, int center_y);

#ifdef __cplusplus
}
#endif
