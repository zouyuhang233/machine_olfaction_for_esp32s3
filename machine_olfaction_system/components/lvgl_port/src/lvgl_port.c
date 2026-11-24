#include "lvgl_port.h"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lcd.h"
#include "lvgl.h"
#include "esp_timer.h"

#if LV_COLOR_DEPTH != 32
#error "The AiDoGUI project requires LV_COLOR_DEPTH=32 (XRGB8888)"
#endif

static const char *TAG = "lvgl_port";

#define LVGL_TASK_STACK        (24 * 1024)
#define LVGL_TASK_PRIO         5
#define LVGL_TASK_PERIOD_MS    5
#define LVGL_TICK_PERIOD_US    1000
#define DRAW_BUF_LINES         40
#define DRAW_BUF_MAX_WIDTH     240
#define DRAW_BUF_BYTES_PER_PX  4
#define DRAW_BUF_BYTES         (DRAW_BUF_MAX_WIDTH * DRAW_BUF_LINES * DRAW_BUF_BYTES_PER_PX)

static lv_display_t *s_display;
static uint8_t s_draw_buf[DRAW_BUF_BYTES];
static esp_timer_handle_t s_tick_timer;

static void tick_cb(void *arg)
{
    (void)arg;
    lv_tick_inc(1);
}

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    /* LVGL 9 原生 32 位格式为 XRGB8888，内存布局 B,G,R,X。 */
    lcd_flush_area_rgb8888(area->x1, area->y1, area->x2, area->y2,
                           (const uint32_t *)px_map);
    lv_display_flush_ready(disp);
}

static void lvgl_task(void *arg)
{
    (void)arg;
    for (;;) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(LVGL_TASK_PERIOD_MS));
    }
}

esp_err_t lvgl_port_init(void)
{
    lv_init();

    const int hor = lcd_width();
    const int ver = lcd_height();
    if (hor <= 0 || ver <= 0 || hor > 240 || ver > 240) {
        ESP_LOGE(TAG, "屏幕尺寸无效: %dx%d", hor, ver);
        return ESP_ERR_INVALID_STATE;
    }

    s_display = lv_display_create(hor, ver);
    if (s_display == NULL) {
        ESP_LOGE(TAG, "LVGL 显示创建失败");
        return ESP_ERR_NO_MEM;
    }
    lv_display_set_default(s_display);
    lv_display_set_flush_cb(s_display, flush_cb);
    lv_display_set_color_format(s_display, LV_COLOR_FORMAT_XRGB8888);
    lv_display_set_buffers(s_display, s_draw_buf, NULL,
                           (uint32_t)(hor * DRAW_BUF_LINES * DRAW_BUF_BYTES_PER_PX),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    const esp_timer_create_args_t tick_args = {
        .callback = tick_cb,
        .name = "lvgl_tick",
    };
    ESP_ERROR_CHECK(esp_timer_create(&tick_args, &s_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(s_tick_timer, LVGL_TICK_PERIOD_US));

    ESP_LOGI(TAG, "LVGL %d.%d.%d 就绪: %dx%d, 色深 %d bit, 绘制缓冲 %d 行",
             LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH,
             hor, ver, LV_COLOR_DEPTH, DRAW_BUF_LINES);
    return ESP_OK;
}

esp_err_t lvgl_port_start(void)
{
    if (xTaskCreate(lvgl_task, "lvgl", LVGL_TASK_STACK, NULL,
                    LVGL_TASK_PRIO, NULL) != pdPASS) {
        ESP_LOGE(TAG, "LVGL 任务创建失败");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

