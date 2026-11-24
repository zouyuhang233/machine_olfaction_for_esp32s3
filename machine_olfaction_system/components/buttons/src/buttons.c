#include "buttons.h"

#include <stdint.h>

#include "board_pins.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

#define DEBOUNCE_US        20000
#define REPEAT_DELAY_US    420000
#define REPEAT_PERIOD_US    90000

static const char *TAG = "buttons";

static const gpio_num_t s_pins[BTN_COUNT] = {
    BOARD_KEY_LEFT, BOARD_KEY_RIGHT, BOARD_KEY_OK, BOARD_KEY_BACK,
};
static bool s_raw[BTN_COUNT];
static bool s_stable[BTN_COUNT];
static int64_t s_changed_us[BTN_COUNT];
static int64_t s_next_repeat_us[BTN_COUNT];

static bool repeats(button_id_t id)
{
    return id == BTN_LEFT || id == BTN_RIGHT;
}

void buttons_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << BOARD_KEY_LEFT) | (1ULL << BOARD_KEY_RIGHT) |
                        (1ULL << BOARD_KEY_OK) | (1ULL << BOARD_KEY_BACK),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "按键 GPIO 配置失败: %s", esp_err_to_name(err));
        return;
    }

    int64_t now = esp_timer_get_time();
    for (int i = 0; i < BTN_COUNT; i++) {
        s_raw[i] = false;
        s_stable[i] = false;
        s_changed_us[i] = now;
        s_next_repeat_us[i] = 0;
    }
}

bool buttons_poll_event(button_event_t *out)
{
    const int64_t now = esp_timer_get_time();
    for (int i = 0; i < BTN_COUNT; i++) {
        const bool raw = (gpio_get_level(s_pins[i]) == 0);  /* 按下 = 低 */
        if (raw != s_raw[i]) {
            s_raw[i] = raw;
            s_changed_us[i] = now;
        }
        if (s_stable[i] != raw && (now - s_changed_us[i]) >= DEBOUNCE_US) {
            s_stable[i] = raw;
            if (raw) {
                if (out != NULL) {
                    out->id = (button_id_t)i;
                    out->repeat = false;
                }
                s_next_repeat_us[i] = now + REPEAT_DELAY_US;
                return true;
            } else {
                s_next_repeat_us[i] = 0;
            }
        }

        /* 左右键支持按住连续滚动；确认/返回只触发一次。 */
        if (raw && s_stable[i] && repeats((button_id_t)i) &&
            s_next_repeat_us[i] != 0 && now >= s_next_repeat_us[i]) {
            s_next_repeat_us[i] += REPEAT_PERIOD_US;
            if (out != NULL) {
                out->id = (button_id_t)i;
                out->repeat = true;
            }
            return true;
        }
    }
    return false;
}

bool buttons_poll(button_id_t *out)
{
    button_event_t event;
    if (!buttons_poll_event(&event)) {
        return false;
    }
    if (out != NULL) {
        *out = event.id;
    }
    return true;
}

void buttons_cancel_repeat(void)
{
    for (int i = 0; i < BTN_COUNT; i++) {
        s_next_repeat_us[i] = 0;
    }
}
