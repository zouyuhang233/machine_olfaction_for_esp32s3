#include "app_ui.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "bsp.h"
#include "buttons.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "lcd.h"
#include "services.h"
#include "settings_store.h"
#include "ui.h"
#include "galaxy_effect.h"

static const char *TAG = "app_ui";

#define UI_WIDTH 240
#define UI_HEIGHT 240
#define FOCUS_ANIM_MS 120
#define SCREEN_LOCK_MS 360

typedef enum {
    SCR_NONE = 0,
    SCR_HOME,
    SCR_DETECTION,
    SCR_RESULT,
    SCR_MENU_DARK,
    SCR_MENU_LIGHT,
    SCR_SENSOR_DARK,
    SCR_SENSOR_LIGHT,
    SCR_NETWORK_DARK,
    SCR_NETWORK_LIGHT,
    SCR_BLE_DARK,
    SCR_BLE_LIGHT,
    SCR_AUDIO_DARK,
    SCR_AUDIO_LIGHT,
    SCR_DISPLAY_DARK,
    SCR_DISPLAY_LIGHT,
    SCR_SYSTEM_DARK,
    SCR_SYSTEM_LIGHT,
    SCR_COUNT,
} ui_screen_id_t;

typedef enum {
    EDIT_NONE = 0,
    EDIT_VOLUME,
    EDIT_BRIGHTNESS,
} edit_mode_t;

typedef enum {
    CONFIRM_NONE = 0,
    CONFIRM_REBOOT,
    CONFIRM_RESET,
} confirm_mode_t;

static ui_screen_id_t s_screen_id;
static int s_focus_index;
static int s_focus_memory[SCR_COUNT];
static lv_obj_t *s_focus_target;
static lv_obj_t *s_focus_ring;
static uint32_t s_input_lock_until;
static bool s_screen_off;
static bool s_light_theme;

static edit_mode_t s_edit_mode;
static bool s_audio_dirty;
static bool s_brightness_dirty;

static lv_obj_t *s_toast;
static lv_timer_t *s_toast_timer;
static lv_timer_t *s_measure_timer;

static confirm_mode_t s_confirm_mode;
static int s_confirm_choice;
static lv_obj_t *s_confirm_overlay;
static lv_obj_t *s_confirm_buttons[2];
static lv_obj_t *s_confirm_labels[2];

static lv_timer_t *s_input_timer;
static lv_timer_t *s_value_timer;

#ifndef APP_UI_SELF_TEST
#define APP_UI_SELF_TEST 0
#endif

static bool object_ok(lv_obj_t *obj)
{
    return obj != NULL && lv_obj_is_valid(obj);
}

static void set_label_text(lv_obj_t *obj, const char *text)
{
    if (object_ok(obj) && text != NULL) {
        lv_label_set_text(obj, text);
    }
}

static void set_switch_checked(lv_obj_t *obj, bool checked)
{
    if (!object_ok(obj)) return;
    if (checked) lv_obj_add_state(obj, LV_STATE_CHECKED);
    else lv_obj_remove_state(obj, LV_STATE_CHECKED);
}

static void set_slider_value(lv_obj_t *obj, int32_t value)
{
    if (object_ok(obj)) lv_slider_set_value(obj, value, LV_ANIM_OFF);
}

static bool screen_uses_light(ui_screen_id_t id)
{
    switch (id) {
    case SCR_MENU_LIGHT:
    case SCR_SENSOR_LIGHT:
    case SCR_NETWORK_LIGHT:
    case SCR_BLE_LIGHT:
    case SCR_AUDIO_LIGHT:
    case SCR_DISPLAY_LIGHT:
    case SCR_SYSTEM_LIGHT:
        return true;
    default:
        return false;
    }
}

static bool screen_sets_dark_theme(ui_screen_id_t id)
{
    switch (id) {
    case SCR_MENU_DARK:
    case SCR_SENSOR_DARK:
    case SCR_NETWORK_DARK:
    case SCR_BLE_DARK:
    case SCR_AUDIO_DARK:
    case SCR_DISPLAY_DARK:
    case SCR_SYSTEM_DARK:
        return true;
    default:
        return false;
    }
}


static ui_screen_id_t active_screen_id(void)
{
    lv_obj_t *scr = lv_screen_active();
    if (scr == aidogui_ui.screen1.root) return SCR_HOME;
    if (scr == aidogui_ui.detection.root) return SCR_DETECTION;
    if (scr == aidogui_ui.result.root) return SCR_RESULT;
    if (scr == aidogui_ui.menu.root) return SCR_MENU_DARK;
    if (scr == aidogui_ui.menulight.root) return SCR_MENU_LIGHT;
    if (scr == aidogui_ui.sensordark.root) return SCR_SENSOR_DARK;
    if (scr == aidogui_ui.sensorlight.root) return SCR_SENSOR_LIGHT;
    if (scr == aidogui_ui.networkdark.root) return SCR_NETWORK_DARK;
    if (scr == aidogui_ui.networklight.root) return SCR_NETWORK_LIGHT;
    if (scr == aidogui_ui.bledark.root) return SCR_BLE_DARK;
    if (scr == aidogui_ui.blelight.root) return SCR_BLE_LIGHT;
    if (scr == aidogui_ui.audiodark.root) return SCR_AUDIO_DARK;
    if (scr == aidogui_ui.audiolight.root) return SCR_AUDIO_LIGHT;
    if (scr == aidogui_ui.displaydark.root) return SCR_DISPLAY_DARK;
    if (scr == aidogui_ui.displaylight.root) return SCR_DISPLAY_LIGHT;
    if (scr == aidogui_ui.systemdark.root) return SCR_SYSTEM_DARK;
    if (scr == aidogui_ui.systemlight.root) return SCR_SYSTEM_LIGHT;
    return SCR_NONE;
}

static int focus_count_for(ui_screen_id_t id)
{
    switch (id) {
    case SCR_HOME:
    case SCR_RESULT:
        return 1;
    case SCR_MENU_DARK:
    case SCR_MENU_LIGHT:
        return 6;
    case SCR_NETWORK_DARK:
    case SCR_NETWORK_LIGHT:
    case SCR_BLE_DARK:
    case SCR_BLE_LIGHT:
        return 1;
    case SCR_AUDIO_DARK:
    case SCR_AUDIO_LIGHT:
        return 3;
    case SCR_DISPLAY_DARK:
    case SCR_DISPLAY_LIGHT:
        return 4;
    case SCR_SYSTEM_DARK:
    case SCR_SYSTEM_LIGHT:
        return 3;
    default:
        return 0;
    }
}

static int default_focus_for(ui_screen_id_t id)
{
    (void)id;
    return 0;
}

static lv_obj_t *focus_target_for(ui_screen_id_t id, int index)
{
    switch (id) {
    case SCR_HOME: return aidogui_ui.screen1.RingConfirm;
    case SCR_RESULT: return aidogui_ui.result.ResultConfirm;
    case SCR_MENU_DARK:
        switch (index) {
        case 0: return aidogui_ui.menu.MenuSensor;
        case 1: return aidogui_ui.menu.MenuNetwork;
        case 2: return aidogui_ui.menu.MenuBle;
        case 3: return aidogui_ui.menu.MenuAudio;
        case 4: return aidogui_ui.menu.MenuDisplay;
        case 5: return aidogui_ui.menu.MenuSystem;
        default: return NULL;
        }
    case SCR_MENU_LIGHT:
        switch (index) {
        case 0: return aidogui_ui.menulight.LightSensor;
        case 1: return aidogui_ui.menulight.LightNetwork;
        case 2: return aidogui_ui.menulight.LightBle;
        case 3: return aidogui_ui.menulight.LightAudio;
        case 4: return aidogui_ui.menulight.LightDisplay;
        case 5: return aidogui_ui.menulight.LightSystem;
        default: return NULL;
        }
    case SCR_NETWORK_DARK: return aidogui_ui.networkdark.WifiRow;
    case SCR_NETWORK_LIGHT: return aidogui_ui.networklight.WifiRow;
    case SCR_BLE_DARK: return aidogui_ui.bledark.BleRow;
    case SCR_BLE_LIGHT: return aidogui_ui.blelight.BleRow;
    case SCR_AUDIO_DARK:
        switch (index) {
        case 0: return aidogui_ui.audiodark.VolumeSlider;
        case 1: return aidogui_ui.audiodark.PromptRow;
        case 2: return aidogui_ui.audiodark.TestPlayButton;
        default: return NULL;
        }
    case SCR_AUDIO_LIGHT:
        switch (index) {
        case 0: return aidogui_ui.audiolight.VolumeSlider;
        case 1: return aidogui_ui.audiolight.PromptRow;
        case 2: return aidogui_ui.audiolight.TestPlayButton;
        default: return NULL;
        }
    case SCR_DISPLAY_DARK:
        switch (index) {
        case 0: return aidogui_ui.displaydark.NightButton;
        case 1: return aidogui_ui.displaydark.Row100;
        case 2: return aidogui_ui.displaydark.BacklightSlider;
        case 3: return aidogui_ui.displaydark.SleepRow;
        default: return NULL;
        }
    case SCR_DISPLAY_LIGHT:
        switch (index) {
        case 0: return aidogui_ui.displaylight.DayButton;
        case 1: return aidogui_ui.displaylight.Row100;
        case 2: return aidogui_ui.displaylight.BacklightSlider;
        case 3: return aidogui_ui.displaylight.SleepRow;
        default: return NULL;
        }
    case SCR_SYSTEM_DARK:
        switch (index) {
        case 0: return aidogui_ui.systemdark.Row148;
        case 1: return aidogui_ui.systemdark.RebootButton;
        case 2: return aidogui_ui.systemdark.ResetButton;
        default: return NULL;
        }
    case SCR_SYSTEM_LIGHT:
        switch (index) {
        case 0: return aidogui_ui.systemlight.Row148;
        case 1: return aidogui_ui.systemlight.RebootButton;
        case 2: return aidogui_ui.systemlight.ResetButton;
        default: return NULL;
        }
    default: return NULL;
    }
}

static lv_obj_t *screen_root_for(ui_screen_id_t id)
{
    switch (id) {
    case SCR_HOME: return aidogui_ui.screen1.root;
    case SCR_DETECTION: return aidogui_ui.detection.root;
    case SCR_RESULT: return aidogui_ui.result.root;
    case SCR_MENU_DARK: return aidogui_ui.menu.root;
    case SCR_MENU_LIGHT: return aidogui_ui.menulight.root;
    case SCR_SENSOR_DARK: return aidogui_ui.sensordark.root;
    case SCR_SENSOR_LIGHT: return aidogui_ui.sensorlight.root;
    case SCR_NETWORK_DARK: return aidogui_ui.networkdark.root;
    case SCR_NETWORK_LIGHT: return aidogui_ui.networklight.root;
    case SCR_BLE_DARK: return aidogui_ui.bledark.root;
    case SCR_BLE_LIGHT: return aidogui_ui.blelight.root;
    case SCR_AUDIO_DARK: return aidogui_ui.audiodark.root;
    case SCR_AUDIO_LIGHT: return aidogui_ui.audiolight.root;
    case SCR_DISPLAY_DARK: return aidogui_ui.displaydark.root;
    case SCR_DISPLAY_LIGHT: return aidogui_ui.displaylight.root;
    case SCR_SYSTEM_DARK: return aidogui_ui.systemdark.root;
    case SCR_SYSTEM_LIGHT: return aidogui_ui.systemlight.root;
    default: return NULL;
    }
}

static lv_obj_t *screen_stage_for(ui_screen_id_t id)
{
    switch (id) {
    case SCR_HOME: return aidogui_ui.screen1.HomeStage;
    case SCR_DETECTION: return aidogui_ui.detection.DetectionStage;
    case SCR_RESULT: return aidogui_ui.result.ResultStage;
    case SCR_MENU_DARK: return aidogui_ui.menu.MenuStage;
    case SCR_MENU_LIGHT: return aidogui_ui.menulight.MenuLightStage;
    case SCR_SENSOR_DARK: return aidogui_ui.sensordark.PageStage;
    case SCR_SENSOR_LIGHT: return aidogui_ui.sensorlight.PageStage;
    case SCR_NETWORK_DARK: return aidogui_ui.networkdark.PageStage;
    case SCR_NETWORK_LIGHT: return aidogui_ui.networklight.PageStage;
    case SCR_BLE_DARK: return aidogui_ui.bledark.PageStage;
    case SCR_BLE_LIGHT: return aidogui_ui.blelight.PageStage;
    case SCR_AUDIO_DARK: return aidogui_ui.audiodark.PageStage;
    case SCR_AUDIO_LIGHT: return aidogui_ui.audiolight.PageStage;
    case SCR_DISPLAY_DARK: return aidogui_ui.displaydark.PageStage;
    case SCR_DISPLAY_LIGHT: return aidogui_ui.displaylight.PageStage;
    case SCR_SYSTEM_DARK: return aidogui_ui.systemdark.PageStage;
    case SCR_SYSTEM_LIGHT: return aidogui_ui.systemlight.PageStage;
    default: return NULL;
    }
}

static void init_screen_for(ui_screen_id_t id)
{
    switch (id) {
    case SCR_HOME: ui_screen1_screen_init(); break;
    case SCR_DETECTION: ui_detection_screen_init(); break;
    case SCR_RESULT: ui_result_screen_init(); break;
    case SCR_MENU_DARK: ui_menu_screen_init(); break;
    case SCR_MENU_LIGHT: ui_menulight_screen_init(); break;
    case SCR_SENSOR_DARK: ui_sensordark_screen_init(); break;
    case SCR_SENSOR_LIGHT: ui_sensorlight_screen_init(); break;
    case SCR_NETWORK_DARK: ui_networkdark_screen_init(); break;
    case SCR_NETWORK_LIGHT: ui_networklight_screen_init(); break;
    case SCR_BLE_DARK: ui_bledark_screen_init(); break;
    case SCR_BLE_LIGHT: ui_blelight_screen_init(); break;
    case SCR_AUDIO_DARK: ui_audiodark_screen_init(); break;
    case SCR_AUDIO_LIGHT: ui_audiolight_screen_init(); break;
    case SCR_DISPLAY_DARK: ui_displaydark_screen_init(); break;
    case SCR_DISPLAY_LIGHT: ui_displaylight_screen_init(); break;
    case SCR_SYSTEM_DARK: ui_systemdark_screen_init(); break;
    case SCR_SYSTEM_LIGHT: ui_systemlight_screen_init(); break;
    default: break;
    }
}

static void prepare_screen_for_display(ui_screen_id_t id)
{
    lv_obj_t *root = screen_root_for(id);
    lv_obj_t *stage = screen_stage_for(id);
    if (!object_ok(root) || !object_ok(stage)) return;

    const bool light = screen_uses_light(id);
    lv_obj_set_size(root, UI_WIDTH, UI_HEIGHT);
    lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(root, light ? lv_color_hex(0xEAF1EF) : lv_color_hex(0x05080C), 0);
    lv_obj_set_style_bg_grad_color(root, light ? lv_color_hex(0xE1ECE8) : lv_color_hex(0x091117), 0);
    lv_obj_set_style_bg_grad_dir(root, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_main_stop(root, 0, 0);
    lv_obj_set_style_bg_grad_stop(root, 255, 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_radius(root, 0, 0);
    lv_obj_set_style_pad_all(root, 0, 0);

    if (lv_obj_get_parent(stage) != root) return;

    lv_obj_set_size(stage, UI_WIDTH, UI_HEIGHT);
    lv_obj_set_pos(stage, 0, 0);
    lv_obj_update_layout(root);
}

static lv_color_t focus_accent(void)
{
    return screen_uses_light(s_screen_id) ? lv_color_hex(0x00798B) : lv_color_hex(0x22D3EE);
}

static void focus_ring_apply_theme(void)
{
    if (!object_ok(s_focus_ring)) return;
    lv_color_t accent = focus_accent();
    lv_obj_set_style_border_color(s_focus_ring, accent, 0);
    lv_obj_set_style_shadow_color(s_focus_ring, accent, 0);
}

static void focus_ring_hide(void)
{
    if (object_ok(s_focus_ring)) lv_obj_add_flag(s_focus_ring, LV_OBJ_FLAG_HIDDEN);
    s_focus_target = NULL;
}

static void focus_ring_position(void)
{
    if (!object_ok(s_focus_ring) || !object_ok(s_focus_target) || s_confirm_mode != CONFIRM_NONE) return;

    lv_area_t area;
    lv_obj_get_coords(s_focus_target, &area);
    int32_t w = lv_area_get_width(&area) + 4;
    int32_t h = lv_area_get_height(&area) + 4;
    if (w <= 4 || h <= 4 || w > UI_WIDTH || h > UI_HEIGHT) return;

    int32_t x = area.x1 - 2;
    int32_t y = area.y1 - 2;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x + w > UI_WIDTH) x = UI_WIDTH - w;
    if (y + h > UI_HEIGHT) y = UI_HEIGHT - h;

    lv_obj_set_pos(s_focus_ring, x, y);
    lv_obj_set_size(s_focus_ring, w, h);
}

static void focus_update(bool animate)
{
    if (!object_ok(s_focus_ring) || s_confirm_mode != CONFIRM_NONE) {
        focus_ring_hide();
        return;
    }
    int count = focus_count_for(s_screen_id);
    if (count <= 0) {
        focus_ring_hide();
        return;
    }
    if (s_focus_index < 0) s_focus_index = 0;
    if (s_focus_index >= count) s_focus_index = count - 1;
    lv_obj_t *target = focus_target_for(s_screen_id, s_focus_index);
    if (!object_ok(target)) {
        focus_ring_hide();
        return;
    }
    bool changed = target != s_focus_target;
    s_focus_target = target;
    focus_ring_apply_theme();
    lv_obj_remove_flag(s_focus_ring, LV_OBJ_FLAG_HIDDEN);
    focus_ring_position();
    if (changed || animate) {
        lv_anim_delete(s_focus_ring, NULL);
        lv_obj_set_style_opa(s_focus_ring, LV_OPA_TRANSP, 0);
        lv_obj_fade_in(s_focus_ring, FOCUS_ANIM_MS, 0);
    } else {
        lv_obj_set_style_opa(s_focus_ring, LV_OPA_COVER, 0);
    }
}

static void focus_move(int delta)
{
    int count = focus_count_for(s_screen_id);
    if (count <= 1) return;
    s_focus_index = (s_focus_index + delta + count) % count;
    s_focus_memory[s_screen_id] = s_focus_index;
    focus_update(true);
}

static void refresh_values_for(ui_screen_id_t id);

static void toast_delete(void)
{
    if (s_toast_timer != NULL) {
        lv_timer_del(s_toast_timer);
        s_toast_timer = NULL;
    }
    if (object_ok(s_toast)) lv_obj_del(s_toast);
    s_toast = NULL;
}

static void toast_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    s_toast_timer = NULL;
    if (object_ok(s_toast)) lv_obj_del(s_toast);
    s_toast = NULL;
}

static void show_toast(const char *text)
{
    if (text == NULL) return;
    toast_delete();
    bool light = screen_uses_light(s_screen_id);
    s_toast = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_toast);
    lv_obj_set_size(s_toast, 220, 30);
    lv_obj_align(s_toast, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_remove_flag(s_toast, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_toast, light ? lv_color_white() : lv_color_hex(0x081217), 0);
    lv_obj_set_style_bg_opa(s_toast, 240, 0);
    lv_obj_set_style_radius(s_toast, 4, 0);
    lv_obj_set_style_border_width(s_toast, 1, 0);
    lv_obj_set_style_border_color(s_toast, focus_accent(), 0);

    lv_obj_t *label = lv_label_create(s_toast);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label, light ? lv_color_hex(0x17373D) : lv_color_hex(0xD7E8EB), 0);
    lv_obj_center(label);
    lv_obj_set_style_opa(s_toast, LV_OPA_TRANSP, 0);
    lv_obj_fade_in(s_toast, 100, 0);
    s_toast_timer = lv_timer_create(toast_timer_cb, 1450, NULL);
    lv_timer_set_repeat_count(s_toast_timer, 1);
}

static void finish_edit_mode(bool commit)
{
    if (s_audio_dirty) {
        if (commit) settings_save();
        s_audio_dirty = false;
    }
    if (s_brightness_dirty) {
        if (commit) settings_save();
        s_brightness_dirty = false;
    }
    s_edit_mode = EDIT_NONE;
}

static void close_confirm(void)
{
    if (object_ok(s_confirm_overlay)) lv_obj_del(s_confirm_overlay);
    s_confirm_overlay = NULL;
    s_confirm_buttons[0] = NULL;
    s_confirm_buttons[1] = NULL;
    s_confirm_labels[0] = NULL;
    s_confirm_labels[1] = NULL;
    s_confirm_mode = CONFIRM_NONE;
    focus_update(true);
}

static void update_confirm_visual(void)
{
    bool light = screen_uses_light(s_screen_id);
    for (int i = 0; i < 2; i++) {
        if (!object_ok(s_confirm_buttons[i])) continue;
        bool selected = (s_confirm_choice == i);
        lv_obj_set_style_bg_color(s_confirm_buttons[i],
                                  selected ? focus_accent() : (light ? lv_color_white() : lv_color_hex(0x0A1A20)), 0);
        lv_obj_set_style_border_width(s_confirm_buttons[i], selected ? 0 : 1, 0);
        lv_obj_set_style_border_color(s_confirm_buttons[i],
                                      light ? lv_color_hex(0xC3D4D0) : lv_color_hex(0x17323A), 0);
        lv_obj_set_style_text_color(s_confirm_labels[i],
                                    selected ? (light ? lv_color_white() : lv_color_hex(0x05080C))
                                             : (light ? lv_color_hex(0x17373D) : lv_color_hex(0xD7E8EB)), 0);
    }
}

static void confirm_execute(void)
{
    confirm_mode_t action = s_confirm_mode;
    close_confirm();
    if (action == CONFIRM_RESET) {
        settings_reset_defaults();
        bsp_backlight_set(settings_get()->brightness);
        lcd_set_rotation(settings_get()->rotation);
        lv_obj_invalidate(lv_screen_active());
        lv_obj_invalidate(lv_layer_top());
        refresh_values_for(s_screen_id);
        show_toast("RESET DONE");
    } else if (action == CONFIRM_REBOOT) {
        esp_restart();
    }
}

static void open_confirm(confirm_mode_t mode)
{
    if (mode == CONFIRM_NONE || s_confirm_mode != CONFIRM_NONE) return;
    toast_delete();
    s_confirm_mode = mode;
    s_confirm_choice = 0;
    bool light = screen_uses_light(s_screen_id);
    focus_ring_hide();

    s_confirm_overlay = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_confirm_overlay);
    lv_obj_set_size(s_confirm_overlay, UI_WIDTH, UI_HEIGHT);
    lv_obj_set_pos(s_confirm_overlay, 0, 0);
    lv_obj_remove_flag(s_confirm_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_confirm_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_confirm_overlay, 165, 0);

    lv_obj_t *panel = lv_obj_create(s_confirm_overlay);
    lv_obj_remove_style_all(panel);
    lv_obj_set_size(panel, 210, 112);
    lv_obj_center(panel);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(panel, light ? lv_color_white() : lv_color_hex(0x081217), 0);
    lv_obj_set_style_bg_opa(panel, 255, 0);
    lv_obj_set_style_radius(panel, 6, 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, focus_accent(), 0);
    lv_obj_set_style_shadow_width(panel, 12, 0);
    lv_obj_set_style_shadow_color(panel, lv_color_black(), 0);
    lv_obj_set_style_shadow_opa(panel, 80, 0);

    lv_obj_t *title = lv_label_create(panel);
    lv_label_set_text(title, mode == CONFIRM_REBOOT ? "REBOOT" : "RESET?");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, light ? lv_color_hex(0x17373D) : lv_color_hex(0xD7E8EB), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    static const char *button_text[2] = {"CANCEL", "OK"};
    for (int i = 0; i < 2; i++) {
        s_confirm_buttons[i] = lv_obj_create(panel);
        lv_obj_remove_style_all(s_confirm_buttons[i]);
        lv_obj_set_size(s_confirm_buttons[i], 68, 28);
        lv_obj_align(s_confirm_buttons[i], LV_ALIGN_TOP_LEFT, 34 + i * 74, 46);
        lv_obj_remove_flag(s_confirm_buttons[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_radius(s_confirm_buttons[i], 4, 0);
        s_confirm_labels[i] = lv_label_create(s_confirm_buttons[i]);
        lv_label_set_text(s_confirm_labels[i], button_text[i]);
        lv_obj_set_style_text_font(s_confirm_labels[i], &lv_font_montserrat_16, 0);
        lv_obj_center(s_confirm_labels[i]);
    }

    lv_obj_t *hint = lv_label_create(panel);
    lv_label_set_text(hint, "ARROWS SELECT  OK  BACK");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, light ? lv_color_hex(0x5A7276) : lv_color_hex(0x6D828A), 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -8);
    update_confirm_visual();
    lv_obj_set_style_opa(s_confirm_overlay, LV_OPA_TRANSP, 0);
    lv_obj_fade_in(s_confirm_overlay, 120, 0);
}

static void navigate_home(void)
{
    finish_edit_mode(true);
    init_screen_for(SCR_HOME);
    prepare_screen_for_display(SCR_HOME);
    _ui_screen_change_ex(&aidogui_ui.screen1.root, LV_SCREEN_LOAD_ANIM_FADE_IN, 200, 0, false, true);
    s_input_lock_until = lv_tick_get() + SCREEN_LOCK_MS;
}

static void navigate_to(ui_screen_id_t target, int animation)
{
    finish_edit_mode(true);
    init_screen_for(target);
    prepare_screen_for_display(target);
    switch (target) {
    case SCR_MENU_DARK:
        _ui_screen_change_ex(&aidogui_ui.menu.root, animation, 220, 0, false, true);
        break;
    case SCR_MENU_LIGHT:
        _ui_screen_change_ex(&aidogui_ui.menulight.root, animation, 240, 0, false, true);
        break;
    case SCR_SENSOR_DARK:
        _ui_screen_change_ex(&aidogui_ui.sensordark.root, animation, 220, 0, false, true);
        break;
    case SCR_SENSOR_LIGHT:
        _ui_screen_change_ex(&aidogui_ui.sensorlight.root, animation, 220, 0, false, true);
        break;
    case SCR_NETWORK_DARK:
        _ui_screen_change_ex(&aidogui_ui.networkdark.root, animation, 220, 0, false, true);
        break;
    case SCR_NETWORK_LIGHT:
        _ui_screen_change_ex(&aidogui_ui.networklight.root, animation, 220, 0, false, true);
        break;
    case SCR_BLE_DARK:
        _ui_screen_change_ex(&aidogui_ui.bledark.root, animation, 220, 0, false, true);
        break;
    case SCR_BLE_LIGHT:
        _ui_screen_change_ex(&aidogui_ui.blelight.root, animation, 220, 0, false, true);
        break;
    case SCR_AUDIO_DARK:
        _ui_screen_change_ex(&aidogui_ui.audiodark.root, animation, 220, 0, false, true);
        break;
    case SCR_AUDIO_LIGHT:
        _ui_screen_change_ex(&aidogui_ui.audiolight.root, animation, 220, 0, false, true);
        break;
    case SCR_DISPLAY_DARK:
        _ui_screen_change_ex(&aidogui_ui.displaydark.root, animation, 240, 0, false, true);
        break;
    case SCR_DISPLAY_LIGHT:
        _ui_screen_change_ex(&aidogui_ui.displaylight.root, animation, 240, 0, false, true);
        break;
    case SCR_SYSTEM_DARK:
        _ui_screen_change_ex(&aidogui_ui.systemdark.root, animation, 220, 0, false, true);
        break;
    case SCR_SYSTEM_LIGHT:
        _ui_screen_change_ex(&aidogui_ui.systemlight.root, animation, 220, 0, false, true);
        break;
    default:
        break;
    }
    s_input_lock_until = lv_tick_get() + SCREEN_LOCK_MS;
}

static void navigate_menu(void)
{
    finish_edit_mode(true);
    navigate_to(s_light_theme ? SCR_MENU_LIGHT : SCR_MENU_DARK,
                LV_SCREEN_LOAD_ANIM_FADE_IN);
}

static void cancel_measure_timer(void)
{
    if (s_measure_timer != NULL) {
        lv_timer_del(s_measure_timer);
        s_measure_timer = NULL;
    }
}

static void show_result(void)
{
    cancel_measure_timer();
    init_screen_for(SCR_RESULT);
    prepare_screen_for_display(SCR_RESULT);
    _ui_screen_change_ex(&aidogui_ui.result.root, LV_SCREEN_LOAD_ANIM_FADE_IN, 300, 0, false, true);
    s_input_lock_until = lv_tick_get() + SCREEN_LOCK_MS;
}

static void measure_done_cb(lv_timer_t *timer)
{
    (void)timer;
    s_measure_timer = NULL;
    if (lv_screen_active() == aidogui_ui.detection.root) show_result();
}

static void start_measurement(void)
{
    finish_edit_mode(true);
    init_screen_for(SCR_DETECTION);
    prepare_screen_for_display(SCR_DETECTION);
    _ui_screen_change_ex(&aidogui_ui.detection.root, LV_SCREEN_LOAD_ANIM_FADE_IN, 220, 0, false, true);
    s_input_lock_until = lv_tick_get() + SCREEN_LOCK_MS;
    cancel_measure_timer();
    s_measure_timer = lv_timer_create(measure_done_cb, 3200, NULL);
    lv_timer_set_repeat_count(s_measure_timer, 1);
}

static void cancel_measurement(void)
{
    cancel_measure_timer();
    navigate_home();
}

static void refresh_network(ui_screen_id_t id)
{
    bool enabled = net_wifi_enabled();
    bool connected = net_wifi_connected();
    bool light = id == SCR_NETWORK_LIGHT;
    const char *status = !enabled ? "OFF" : (connected ? "ONLINE" : "WAIT");
    lv_color_t status_color = connected ? (light ? lv_color_hex(0x08745D) : lv_color_hex(0x8BEFD2))
                                        : (enabled ? (light ? lv_color_hex(0x8A5A00) : lv_color_hex(0xFBBF24))
                                                   : (light ? lv_color_hex(0x5A7276) : lv_color_hex(0x6D828A)));
    if (id == SCR_NETWORK_DARK) {
        set_label_text(aidogui_ui.networkdark.Value52, status);
        set_label_text(aidogui_ui.networkdark.Value148, net_ip());
        set_switch_checked(aidogui_ui.networkdark.WifiSwitch, enabled);
        if (object_ok(aidogui_ui.networkdark.Value52)) {
            lv_obj_set_style_text_color(aidogui_ui.networkdark.Value52, status_color, 0);
        }
    } else if (id == SCR_NETWORK_LIGHT) {
        set_label_text(aidogui_ui.networklight.Value52, status);
        set_label_text(aidogui_ui.networklight.Value148, net_ip());
        set_switch_checked(aidogui_ui.networklight.WifiSwitch, enabled);
        if (object_ok(aidogui_ui.networklight.Value52)) {
            lv_obj_set_style_text_color(aidogui_ui.networklight.Value52, status_color, 0);
        }
    }
}

static void refresh_ble(ui_screen_id_t id)
{
    bool enabled = net_ble_enabled();
    bool light = id == SCR_BLE_LIGHT;
    const char *status = enabled ? "广播中" : "OFF";
    lv_color_t color = enabled ? (light ? lv_color_hex(0x08745D) : lv_color_hex(0x8BEFD2))
                               : (light ? lv_color_hex(0x5A7276) : lv_color_hex(0x6D828A));
    if (id == SCR_BLE_DARK) {
        set_label_text(aidogui_ui.bledark.Value100, status);
        set_label_text(aidogui_ui.bledark.Value148, net_ble_name());
        set_switch_checked(aidogui_ui.bledark.BleSwitch, enabled);
        if (object_ok(aidogui_ui.bledark.Value100)) {
            lv_obj_set_style_text_color(aidogui_ui.bledark.Value100, color, 0);
        }
    } else if (id == SCR_BLE_LIGHT) {
        set_label_text(aidogui_ui.blelight.Value100, status);
        set_label_text(aidogui_ui.blelight.Value148, net_ble_name());
        set_switch_checked(aidogui_ui.blelight.BleSwitch, enabled);
        if (object_ok(aidogui_ui.blelight.Value100)) {
            lv_obj_set_style_text_color(aidogui_ui.blelight.Value100, color, 0);
        }
    }
}

static void refresh_audio(ui_screen_id_t id)
{
    char buf[12];
    snprintf(buf, sizeof(buf), "%u%%", audio_volume());
    if (id == SCR_AUDIO_DARK) {
        set_label_text(aidogui_ui.audiodark.VolumeValue, buf);
        set_slider_value(aidogui_ui.audiodark.VolumeSlider, audio_volume());
        set_switch_checked(aidogui_ui.audiodark.PromptSwitch, audio_prompt_enabled());
    } else if (id == SCR_AUDIO_LIGHT) {
        set_label_text(aidogui_ui.audiolight.VolumeValue, buf);
        set_slider_value(aidogui_ui.audiolight.VolumeSlider, audio_volume());
        set_switch_checked(aidogui_ui.audiolight.PromptSwitch, audio_prompt_enabled());
    }
}

static void refresh_display(ui_screen_id_t id)
{
    char buf[12], dir[8];
    snprintf(buf, sizeof(buf), "%u%%", settings_get()->brightness);
    snprintf(dir, sizeof(dir), "%u", (unsigned)(settings_get()->rotation & 3) * 90U);
    if (id == SCR_DISPLAY_DARK) {
        set_label_text(aidogui_ui.displaydark.BacklightValue, buf);
        set_slider_value(aidogui_ui.displaydark.BacklightSlider, settings_get()->brightness);
        set_label_text(aidogui_ui.displaydark.Value100, dir);
    } else if (id == SCR_DISPLAY_LIGHT) {
        set_label_text(aidogui_ui.displaylight.BacklightValue, buf);
        set_slider_value(aidogui_ui.displaylight.BacklightSlider, settings_get()->brightness);
        set_label_text(aidogui_ui.displaylight.Value100, dir);
    }
}

static void refresh_system(ui_screen_id_t id)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)modbus_baud());
    if (id == SCR_SYSTEM_DARK) set_label_text(aidogui_ui.systemdark.Value148, buf);
    else if (id == SCR_SYSTEM_LIGHT) set_label_text(aidogui_ui.systemlight.Value148, buf);
}

static void refresh_sensor(ui_screen_id_t id)
{
    sensor_data_t d;
    sensor_get(&d);

    char temp[12], humi[8], press[12], rs[12];
    if (d.valid && isfinite(d.temp)) {
        snprintf(temp, sizeof(temp), "%.1f°C", d.temp);
    } else {
        snprintf(temp, sizeof(temp), "--");
    }
    if (d.valid && isfinite(d.humi)) {
        snprintf(humi, sizeof(humi), "%.0f%%", d.humi);
    } else {
        snprintf(humi, sizeof(humi), "--");
    }
    if (d.valid && isfinite(d.press)) {
        snprintf(press, sizeof(press), "%.0fhPa", d.press);
    } else {
        snprintf(press, sizeof(press), "--");
    }
    if (d.gas_valid && isfinite(d.rs)) {
        snprintf(rs, sizeof(rs), "%.1fk", d.rs);
    } else {
        snprintf(rs, sizeof(rs), "--");
    }

    /* BME690 本身不输出 TVOC；未接 BSEC 时响应条保持 0。 */
    int resp = 0;
    if (isfinite(d.tvoc)) {
        resp = (int)(d.tvoc / 3.0f);
        if (resp < 0) resp = 0;
        if (resp > 100) resp = 100;
    }

    if (id == SCR_SENSOR_DARK) {
        set_label_text(aidogui_ui.sensordark.MetricVal1, temp);
        set_label_text(aidogui_ui.sensordark.MetricVal2, humi);
        set_label_text(aidogui_ui.sensordark.MetricVal3, press);
        set_label_text(aidogui_ui.sensordark.MetricVal4, rs);
        if (object_ok(aidogui_ui.sensordark.RespBar)) {
            lv_bar_set_value(aidogui_ui.sensordark.RespBar, resp, LV_ANIM_ON);
        }
    } else if (id == SCR_SENSOR_LIGHT) {
        set_label_text(aidogui_ui.sensorlight.MetricVal1, temp);
        set_label_text(aidogui_ui.sensorlight.MetricVal2, humi);
        set_label_text(aidogui_ui.sensorlight.MetricVal3, press);
        set_label_text(aidogui_ui.sensorlight.MetricVal4, rs);
        if (object_ok(aidogui_ui.sensorlight.RespBar)) {
            lv_bar_set_value(aidogui_ui.sensorlight.RespBar, resp, LV_ANIM_ON);
        }
    }
}

static void refresh_detection(void)
{
    sensor_data_t d;
    sensor_get(&d);

    char temp[12], humi[8], rs[12], resp[10];
    if (d.valid && isfinite(d.temp)) {
        snprintf(temp, sizeof(temp), "%.1f°C", d.temp);
    } else {
        snprintf(temp, sizeof(temp), "--");
    }
    if (d.valid && isfinite(d.humi)) {
        snprintf(humi, sizeof(humi), "%.0f%%", d.humi);
    } else {
        snprintf(humi, sizeof(humi), "--");
    }
    if (d.gas_valid && isfinite(d.rs)) {
        snprintf(rs, sizeof(rs), "%.1fk", d.rs);
    } else {
        snprintf(rs, sizeof(rs), "--");
    }
    if (isfinite(d.tvoc)) {
        snprintf(resp, sizeof(resp), "%.2f", d.tvoc / 300.0f);
    } else {
        snprintf(resp, sizeof(resp), "--");
    }

    set_label_text(aidogui_ui.detection.ValTemp, temp);
    set_label_text(aidogui_ui.detection.ValHumi, humi);
    set_label_text(aidogui_ui.detection.ValRs, rs);
    set_label_text(aidogui_ui.detection.ValResp, resp);
}

static void refresh_values_for(ui_screen_id_t id)
{
    switch (id) {
    case SCR_DETECTION: refresh_detection(); break;
    case SCR_SENSOR_DARK:
    case SCR_SENSOR_LIGHT: refresh_sensor(id); break;
    case SCR_NETWORK_DARK:
    case SCR_NETWORK_LIGHT: refresh_network(id); break;
    case SCR_BLE_DARK:
    case SCR_BLE_LIGHT: refresh_ble(id); break;
    case SCR_AUDIO_DARK:
    case SCR_AUDIO_LIGHT: refresh_audio(id); break;
    case SCR_DISPLAY_DARK:
    case SCR_DISPLAY_LIGHT: refresh_display(id); break;
    case SCR_SYSTEM_DARK:
    case SCR_SYSTEM_LIGHT: refresh_system(id); break;
    default: break;
    }
}

static void sync_screen(void)
{
    ui_screen_id_t next = active_screen_id();
    if (next == SCR_NONE || next == s_screen_id) return;
    finish_edit_mode(true);
    int old_screen = s_screen_id;
    if (old_screen > SCR_NONE && old_screen < SCR_COUNT && focus_count_for((ui_screen_id_t)old_screen) > 0) {
        s_focus_memory[old_screen] = s_focus_index;
    }
    s_screen_id = next;
    prepare_screen_for_display(next);

    switch (next) {
    case SCR_HOME:
        galaxy_effect_attach(aidogui_ui.screen1.HomeStage, GALAXY_MODE_HOME, 120, 104);
        break;
    case SCR_DETECTION:
        galaxy_effect_attach(aidogui_ui.detection.DetectionStage, GALAXY_MODE_DETECTION, 120, 116);
        break;
    case SCR_RESULT:
        galaxy_effect_attach(aidogui_ui.result.ResultStage, GALAXY_MODE_RESULT, 120, 108);
        break;
    default:
        galaxy_effect_attach(NULL, GALAXY_MODE_OFF, 120, 120);
        break;
    }

    if (screen_uses_light(next)) s_light_theme = true;
    else if (screen_sets_dark_theme(next)) s_light_theme = false;

    int count = focus_count_for(next);
    s_focus_index = s_focus_memory[next] >= 0 ? s_focus_memory[next] : default_focus_for(next);
    if (count > 0) {
        if (s_focus_index < 0) s_focus_index = 0;
        if (s_focus_index >= count) s_focus_index = count - 1;
    }
    s_focus_target = NULL;
    uint32_t min_lock = lv_tick_get() + SCREEN_LOCK_MS;
    if (s_input_lock_until < min_lock) s_input_lock_until = min_lock;
    refresh_values_for(next);
    focus_update(true);
    ESP_LOGI(TAG, "screen=%d focus=%d light=%d heap_int=%u psram=%u",
             (int)next, s_focus_index, s_light_theme,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}

static void toggle_wifi(void)
{
    net_wifi_set_enabled(!net_wifi_enabled());
    refresh_network(s_screen_id);
    show_toast(net_wifi_enabled() ? "WIFI ON" : "WIFI OFF");
}

static void toggle_ble(void)
{
    net_ble_set_enabled(!net_ble_enabled());
    refresh_ble(s_screen_id);
    show_toast(net_ble_enabled() ? "BLE ON" : "BLE OFF");
}

static void audio_adjust(int delta)
{
    int volume = (int)audio_volume() + delta;
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    settings_get()->volume = (uint8_t)volume;
    s_audio_dirty = true;
    refresh_audio(s_screen_id);
}

static void brightness_adjust(int delta)
{
    int value = (int)settings_get()->brightness + delta;
    if (value < 10) value = 10;
    if (value > 100) value = 100;
    settings_get()->brightness = (uint8_t)value;
    bsp_backlight_set((uint8_t)value);
    s_brightness_dirty = true;
    refresh_display(s_screen_id);
}

static void rotate_display(int direction)
{
    app_settings_t *cfg = settings_get();
    int step = direction < 0 ? 3 : 1;
    cfg->rotation = (cfg->rotation + step) & 3;
    settings_save();
    lcd_set_rotation(cfg->rotation);
    lv_obj_invalidate(lv_screen_active());
    lv_obj_invalidate(lv_layer_top());
    refresh_display(s_screen_id);
    show_toast("ROTATE");
}

static void cycle_baud(int direction)
{
    static const uint32_t bauds[] = {9600, 19200, 38400, 57600, 115200};
    const int count = (int)(sizeof(bauds) / sizeof(bauds[0]));
    uint32_t current = modbus_baud();
    int next = 0;
    for (int i = 0; i < count; i++) {
        if (bauds[i] == current) {
            next = (i + direction + count) % count;
            break;
        }
    }
    modbus_set_baud(bauds[next]);
    refresh_system(s_screen_id);
    char text[24];
    snprintf(text, sizeof(text), "BAUD %lu", (unsigned long)bauds[next]);
    show_toast(text);
}

static void handle_confirm_key(button_id_t key, bool repeat)
{
    if (key == BTN_UP || key == BTN_DOWN || key == BTN_LEFT || key == BTN_RIGHT) {
        if (!repeat) {
            s_confirm_choice ^= 1;
            update_confirm_visual();
        }
    } else if (key == BTN_OK) {
        if (s_confirm_choice == 0) close_confirm();
        else confirm_execute();
    } else if (key == BTN_BACK) {
        close_confirm();
    }
}

static void handle_menu_key(button_id_t key)
{
    bool light = screen_uses_light(s_screen_id);
    if (key == BTN_UP || key == BTN_LEFT) {
        focus_move(-1);
    } else if (key == BTN_DOWN || key == BTN_RIGHT) {
        focus_move(1);
    } else if (key == BTN_OK) {
        switch (s_focus_index) {
        case 0:
            navigate_to(light ? SCR_SENSOR_LIGHT : SCR_SENSOR_DARK,
                        LV_SCREEN_LOAD_ANIM_MOVE_RIGHT);
            break;
        case 1:
            navigate_to(light ? SCR_NETWORK_LIGHT : SCR_NETWORK_DARK,
                        LV_SCREEN_LOAD_ANIM_MOVE_RIGHT);
            break;
        case 2:
            navigate_to(light ? SCR_BLE_LIGHT : SCR_BLE_DARK,
                        LV_SCREEN_LOAD_ANIM_MOVE_RIGHT);
            break;
        case 3:
            navigate_to(light ? SCR_AUDIO_LIGHT : SCR_AUDIO_DARK,
                        LV_SCREEN_LOAD_ANIM_MOVE_RIGHT);
            break;
        case 4:
            navigate_to(light ? SCR_DISPLAY_LIGHT : SCR_DISPLAY_DARK,
                        LV_SCREEN_LOAD_ANIM_MOVE_RIGHT);
            break;
        case 5:
            navigate_to(light ? SCR_SYSTEM_LIGHT : SCR_SYSTEM_DARK,
                        LV_SCREEN_LOAD_ANIM_MOVE_RIGHT);
            break;
        default:
            break;
        }
    } else if (key == BTN_BACK) {
        navigate_home();
    }
}

static void handle_audio_key(button_id_t key, bool repeat)
{
    (void)repeat;
    if (s_edit_mode == EDIT_VOLUME) {
        if (key == BTN_UP || key == BTN_RIGHT) audio_adjust(5);
        else if (key == BTN_DOWN || key == BTN_LEFT) audio_adjust(-5);
        else if (key == BTN_OK || key == BTN_BACK) finish_edit_mode(true);
        return;
    }
    if (key == BTN_UP) {
        focus_move(-1);
    } else if (key == BTN_DOWN) {
        focus_move(1);
    } else if (key == BTN_LEFT || key == BTN_RIGHT) {
        if (s_focus_index == 0) {
            audio_adjust(key == BTN_RIGHT ? 5 : -5);
        } else if (s_focus_index == 1) {
            audio_set_prompt_enabled(key == BTN_RIGHT);
            refresh_audio(s_screen_id);
        } else if (s_focus_index == 2) {
            audio_play_test();
            show_toast("TEST PLAY");
        }
    } else if (key == BTN_OK) {
        if (s_focus_index == 0) {
            s_edit_mode = EDIT_VOLUME;
            show_toast("ARROWS ADJUST  OK SAVE");
        } else if (s_focus_index == 1) {
            audio_set_prompt_enabled(!audio_prompt_enabled());
            refresh_audio(s_screen_id);
            show_toast(audio_prompt_enabled() ? "PROMPT ON" : "PROMPT OFF");
        } else if (s_focus_index == 2) {
            audio_play_test();
            show_toast("TEST PLAY");
        }
    } else if (key == BTN_BACK) {
        navigate_to(screen_uses_light(s_screen_id) ? SCR_MENU_LIGHT : SCR_MENU_DARK,
                    LV_SCREEN_LOAD_ANIM_MOVE_LEFT);
    }
}

static void handle_display_key(button_id_t key, bool repeat)
{
    (void)repeat;
    if (s_edit_mode == EDIT_BRIGHTNESS) {
        if (key == BTN_UP || key == BTN_RIGHT) brightness_adjust(10);
        else if (key == BTN_DOWN || key == BTN_LEFT) brightness_adjust(-10);
        else if (key == BTN_OK || key == BTN_BACK) finish_edit_mode(true);
        return;
    }
    if (key == BTN_UP) {
        focus_move(-1);
    } else if (key == BTN_DOWN) {
        focus_move(1);
    } else if (key == BTN_LEFT || key == BTN_RIGHT) {
        if (s_focus_index == 0) {
            bool light = key == BTN_RIGHT;
            if (light != screen_uses_light(s_screen_id)) {
                s_light_theme = light;
                s_focus_memory[light ? SCR_DISPLAY_LIGHT : SCR_DISPLAY_DARK] = 0;
                navigate_to(light ? SCR_DISPLAY_LIGHT : SCR_DISPLAY_DARK,
                            LV_SCREEN_LOAD_ANIM_FADE_IN);
            }
        } else if (s_focus_index == 1) {
            rotate_display(key == BTN_RIGHT ? 1 : -1);
        } else if (s_focus_index == 2) {
            brightness_adjust(key == BTN_RIGHT ? 10 : -10);
        } else if (s_focus_index == 3) {
            app_ui_screen_off();
        }
    } else if (key == BTN_OK) {
        if (s_focus_index == 0) {
            bool light = screen_uses_light(s_screen_id);
            s_light_theme = !light;
            s_focus_memory[light ? SCR_DISPLAY_DARK : SCR_DISPLAY_LIGHT] = 0;
            navigate_to(light ? SCR_DISPLAY_DARK : SCR_DISPLAY_LIGHT,
                        LV_SCREEN_LOAD_ANIM_FADE_IN);
        } else if (s_focus_index == 1) {
            rotate_display(1);
        } else if (s_focus_index == 2) {
            s_edit_mode = EDIT_BRIGHTNESS;
            show_toast("ARROWS ADJUST  OK SAVE");
        } else if (s_focus_index == 3) {
            app_ui_screen_off();
        }
    } else if (key == BTN_BACK) {
        navigate_to(screen_uses_light(s_screen_id) ? SCR_MENU_LIGHT : SCR_MENU_DARK,
                    LV_SCREEN_LOAD_ANIM_MOVE_LEFT);
    }
}

static void handle_system_key(button_id_t key)
{
    if (key == BTN_UP) {
        focus_move(-1);
    } else if (key == BTN_DOWN) {
        focus_move(1);
    } else if (key == BTN_LEFT || key == BTN_RIGHT) {
        if (s_focus_index == 0) {
            cycle_baud(key == BTN_RIGHT ? 1 : -1);
        } else {
            focus_move(key == BTN_LEFT ? -1 : 1);
        }
    } else if (key == BTN_OK) {
        if (s_focus_index == 0) cycle_baud(1);
        else if (s_focus_index == 1) open_confirm(CONFIRM_REBOOT);
        else if (s_focus_index == 2) open_confirm(CONFIRM_RESET);
    } else if (key == BTN_BACK) {
        navigate_to(screen_uses_light(s_screen_id) ? SCR_MENU_LIGHT : SCR_MENU_DARK,
                    LV_SCREEN_LOAD_ANIM_MOVE_LEFT);
    }
}

static void handle_key(button_id_t key, bool repeat)
{
    if (s_confirm_mode != CONFIRM_NONE) {
        handle_confirm_key(key, repeat);
        return;
    }
    if (lv_tick_get() < s_input_lock_until) return;

    switch (s_screen_id) {
    case SCR_HOME:
        if (key == BTN_OK) start_measurement();
        else if (key == BTN_BACK) navigate_menu();
        break;
    case SCR_DETECTION:
        if (key == BTN_OK || key == BTN_RIGHT) show_result();
        else if (key == BTN_BACK || key == BTN_LEFT) cancel_measurement();
        break;
    case SCR_RESULT:
        if (key == BTN_OK || key == BTN_RIGHT) navigate_menu();
        else if (key == BTN_BACK || key == BTN_LEFT) navigate_home();
        break;
    case SCR_MENU_DARK:
    case SCR_MENU_LIGHT:
        handle_menu_key(key);
        break;
    case SCR_SENSOR_DARK:
        if (key == BTN_BACK || key == BTN_LEFT) navigate_to(SCR_MENU_DARK, LV_SCREEN_LOAD_ANIM_MOVE_LEFT);
        break;
    case SCR_SENSOR_LIGHT:
        if (key == BTN_BACK || key == BTN_LEFT) navigate_to(SCR_MENU_LIGHT, LV_SCREEN_LOAD_ANIM_MOVE_LEFT);
        break;
    case SCR_NETWORK_DARK:
    case SCR_NETWORK_LIGHT:
        if (key == BTN_OK) toggle_wifi();
        else if (key == BTN_LEFT && net_wifi_enabled()) {
            net_wifi_set_enabled(false);
            refresh_network(s_screen_id);
            show_toast("WIFI OFF");
        } else if (key == BTN_RIGHT && !net_wifi_enabled()) {
            net_wifi_set_enabled(true);
            refresh_network(s_screen_id);
            show_toast("WIFI ON");
        }
        else if (key == BTN_BACK) {
            navigate_to(s_screen_id == SCR_NETWORK_DARK ? SCR_MENU_DARK : SCR_MENU_LIGHT,
                        LV_SCREEN_LOAD_ANIM_MOVE_LEFT);
        }
        break;
    case SCR_BLE_DARK:
    case SCR_BLE_LIGHT:
        if (key == BTN_OK) toggle_ble();
        else if (key == BTN_LEFT && net_ble_enabled()) {
            net_ble_set_enabled(false);
            refresh_ble(s_screen_id);
            show_toast("BLE OFF");
        } else if (key == BTN_RIGHT && !net_ble_enabled()) {
            net_ble_set_enabled(true);
            refresh_ble(s_screen_id);
            show_toast("BLE ON");
        }
        else if (key == BTN_BACK) {
            navigate_to(s_screen_id == SCR_BLE_DARK ? SCR_MENU_DARK : SCR_MENU_LIGHT,
                        LV_SCREEN_LOAD_ANIM_MOVE_LEFT);
        }
        break;
    case SCR_AUDIO_DARK:
    case SCR_AUDIO_LIGHT:
        handle_audio_key(key, repeat);
        break;
    case SCR_DISPLAY_DARK:
    case SCR_DISPLAY_LIGHT:
        handle_display_key(key, repeat);
        break;
    case SCR_SYSTEM_DARK:
    case SCR_SYSTEM_LIGHT:
        handle_system_key(key);
        break;
    default:
        break;
    }
}

static void input_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    sync_screen();
    button_event_t event;
    while (buttons_poll_event(&event)) {
        if (s_screen_off) {
            s_screen_off = false;
            bsp_backlight_set(settings_get()->brightness);
            buttons_cancel_repeat();
            continue;
        }
        handle_key(event.id, event.repeat);
    }
    sync_screen();
    if (s_confirm_mode == CONFIRM_NONE) focus_ring_position();
}

static void value_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    services_update();
    refresh_values_for(s_screen_id);
}

static void create_focus_ring(void)
{
    s_focus_ring = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_focus_ring);
    lv_obj_remove_flag(s_focus_ring, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_focus_ring, LV_OBJ_FLAG_IGNORE_LAYOUT | LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_opa(s_focus_ring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(s_focus_ring, 6, 0);
    lv_obj_set_style_border_width(s_focus_ring, 2, 0);
    lv_obj_set_style_border_opa(s_focus_ring, 255, 0);
    lv_obj_set_style_shadow_width(s_focus_ring, 8, 0);
    lv_obj_set_style_shadow_opa(s_focus_ring, 60, 0);
    lv_obj_set_style_shadow_offset_y(s_focus_ring, 0, 0);
    focus_ring_apply_theme();
}

#if APP_UI_SELF_TEST
static void selftest_timer_cb(lv_timer_t *timer)
{
    static int step;
    static int cycle;
    const int steps = 32;
    bool light = true;
    switch (step) {
    case 0: s_light_theme = false; navigate_menu(); break;
    case 1: navigate_to(SCR_SENSOR_DARK, LV_SCREEN_LOAD_ANIM_MOVE_RIGHT); break;
    case 2: navigate_to(SCR_MENU_DARK, LV_SCREEN_LOAD_ANIM_MOVE_LEFT); break;
    case 3: navigate_to(SCR_NETWORK_DARK, LV_SCREEN_LOAD_ANIM_MOVE_RIGHT); break;
    case 4: navigate_to(SCR_MENU_DARK, LV_SCREEN_LOAD_ANIM_MOVE_LEFT); break;
    case 5: navigate_to(SCR_BLE_DARK, LV_SCREEN_LOAD_ANIM_MOVE_RIGHT); break;
    case 6: navigate_to(SCR_MENU_DARK, LV_SCREEN_LOAD_ANIM_MOVE_LEFT); break;
    case 7: navigate_to(SCR_AUDIO_DARK, LV_SCREEN_LOAD_ANIM_MOVE_RIGHT); break;
    case 8: navigate_to(SCR_MENU_DARK, LV_SCREEN_LOAD_ANIM_MOVE_LEFT); break;
    case 9: navigate_to(SCR_DISPLAY_DARK, LV_SCREEN_LOAD_ANIM_MOVE_RIGHT); break;
    case 10: navigate_to(SCR_MENU_DARK, LV_SCREEN_LOAD_ANIM_MOVE_LEFT); break;
    case 11: navigate_to(SCR_SYSTEM_DARK, LV_SCREEN_LOAD_ANIM_MOVE_RIGHT); break;
    case 12: navigate_to(SCR_MENU_DARK, LV_SCREEN_LOAD_ANIM_MOVE_LEFT); break;
    case 13: s_light_theme = light; navigate_menu(); break;
    case 14: navigate_to(SCR_SENSOR_LIGHT, LV_SCREEN_LOAD_ANIM_MOVE_RIGHT); break;
    case 15: navigate_to(SCR_MENU_LIGHT, LV_SCREEN_LOAD_ANIM_MOVE_LEFT); break;
    case 16: navigate_to(SCR_NETWORK_LIGHT, LV_SCREEN_LOAD_ANIM_MOVE_RIGHT); break;
    case 17: navigate_to(SCR_MENU_LIGHT, LV_SCREEN_LOAD_ANIM_MOVE_LEFT); break;
    case 18: navigate_to(SCR_BLE_LIGHT, LV_SCREEN_LOAD_ANIM_MOVE_RIGHT); break;
    case 19: navigate_to(SCR_MENU_LIGHT, LV_SCREEN_LOAD_ANIM_MOVE_LEFT); break;
    case 20: navigate_to(SCR_AUDIO_LIGHT, LV_SCREEN_LOAD_ANIM_MOVE_RIGHT); break;
    case 21: navigate_to(SCR_MENU_LIGHT, LV_SCREEN_LOAD_ANIM_MOVE_LEFT); break;
    case 22: navigate_to(SCR_DISPLAY_LIGHT, LV_SCREEN_LOAD_ANIM_MOVE_RIGHT); break;
    case 23: navigate_to(SCR_MENU_LIGHT, LV_SCREEN_LOAD_ANIM_MOVE_LEFT); break;
    case 24: navigate_to(SCR_SYSTEM_LIGHT, LV_SCREEN_LOAD_ANIM_MOVE_RIGHT); break;
    case 25: navigate_to(SCR_MENU_LIGHT, LV_SCREEN_LOAD_ANIM_MOVE_LEFT); break;
    case 26: s_light_theme = false; navigate_menu(); break;
    case 27: navigate_home(); break;
    case 28: start_measurement(); break;
    case 29: break;
    case 30: break;
    case 31: navigate_home(); break;
    default: break;
    }
    ESP_LOGI(TAG, "SELFTEST cycle=%d step=%d heap_int=%u", cycle, step,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    step++;
    if (step >= steps) {
        step = 0;
        cycle++;
        if (cycle >= 1) {
            lv_timer_del(timer);
            ESP_LOGI(TAG, "SELFTEST COMPLETE heap_int=%u psram=%u",
                     (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                     (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
        }
    }
}
#endif

esp_err_t app_ui_init(void)
{
    for (int i = 0; i < SCR_COUNT; i++) s_focus_memory[i] = -1;
    s_screen_id = SCR_NONE;
    s_focus_index = 0;
    s_focus_target = NULL;
    s_edit_mode = EDIT_NONE;
    s_light_theme = false;
    s_input_lock_until = 0;
    s_screen_off = false;

    buttons_init();
    aidogui_init();
    if (!object_ok(aidogui_ui.screen1.root)) {
        ESP_LOGE(TAG, "AiDoGUI 初始化失败");
        return ESP_FAIL;
    }

    galaxy_effect_init();
    create_focus_ring();
    sync_screen();
    show_toast("ARROWS SELECT  OK  BACK");

    s_input_timer = lv_timer_create(input_timer_cb, 10, NULL);
    s_value_timer = lv_timer_create(value_timer_cb, 500, NULL);
    if (s_input_timer == NULL || s_value_timer == NULL) {
        ESP_LOGE(TAG, "UI 定时器创建失败");
        return ESP_ERR_NO_MEM;
    }
#if APP_UI_SELF_TEST
    lv_timer_create(selftest_timer_cb, 900, NULL);
#endif
    ESP_LOGI(TAG, "AiDoGUI UI + 6 键适配已启动");
    return ESP_OK;
}

void app_ui_screen_off(void)
{
    s_screen_off = true;
    bsp_backlight(false);
}
