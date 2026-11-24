/*
 * galaxy_effect.c -- lightweight 3D-looking accretion disk for the sensor core.
 */
#include "galaxy_effect.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#include "esp_log.h"

static const char *TAG = "galaxy";

#define GALAXY_PARTICLE_COUNT 46
#define GALAXY_TIMER_MS 33
#define GALAXY_PI 3.14159265358979323846f
#define GALAXY_TAU (GALAXY_PI * 2.0f)
#define GALAXY_FAR 0
#define GALAXY_MID 1
#define GALAXY_NEAR 2

typedef struct {
    lv_obj_t *obj;
    float radius;
    float angle;
    float inward;
    float spin;
    float size;
    float twinkle;
    float wobble;
    uint8_t layer;
    uint8_t color_index;
} galaxy_particle_t;

static galaxy_particle_t s_particles[GALAXY_PARTICLE_COUNT];
static lv_obj_t *s_root;
static lv_obj_t *s_disk_outer;
static lv_obj_t *s_disk_inner;
static lv_timer_t *s_timer;
static galaxy_mode_t s_mode = GALAXY_MODE_OFF;
static int s_cx = 120;
static int s_cy = 110;
static float s_speed = 0.6f;
static float s_spin = 0.6f;
static float s_brightness = 0.7f;
static float s_max_radius = 100.0f;
static float s_disk_angle_outer;
static float s_disk_angle_inner;
static float s_time_acc;
static uint32_t s_rng = 0x71C3A5E1u;

static const uint32_t s_colors[] = {
    0x155766, 0x1D8A9B, 0x22D3EE, 0x89F1FF, 0xD8FBFF, 0x5EEAD4,
};

static bool galaxy_root_valid(void)
{
    return s_root != NULL && lv_obj_is_valid(s_root);
}

static uint32_t galaxy_random(void)
{
    uint32_t x = s_rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s_rng = x != 0 ? x : 0x71C3A5E1u;
    return x;
}

static float galaxy_random_float(void)
{
    return (float)(galaxy_random() & 0xFFFFu) / 65535.0f;
}

static void galaxy_clear_references(void)
{
    s_root = NULL;
    s_disk_outer = NULL;
    s_disk_inner = NULL;
    memset(s_particles, 0, sizeof(s_particles));
}
static lv_obj_t *galaxy_make_disk(lv_obj_t *parent, int size, uint32_t color,
                                  int width, int start_angle, int end_angle, lv_opa_t opa)
{
    lv_obj_t *arc = lv_arc_create(parent);
    lv_obj_remove_style_all(arc);
    lv_obj_set_size(arc, size, size);
    lv_obj_set_pos(arc, s_cx - size / 2, s_cy - size / 2);
    lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(arc, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(arc, 0, 0);
    lv_obj_set_style_pad_all(arc, 0, 0);
    lv_obj_set_style_arc_opa(arc, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, width, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, lv_color_hex(color), LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(arc, opa, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, false, LV_PART_INDICATOR);
    lv_arc_set_bg_angles(arc, 0, 360);
    lv_arc_set_angles(arc, start_angle, end_angle);
    lv_obj_set_style_transform_pivot_x(arc, size / 2, 0);
    lv_obj_set_style_transform_pivot_y(arc, size / 2, 0);
    lv_obj_set_style_transform_scale_y(arc, 100, 0);
    return arc;
}

static void galaxy_reseed_particle(galaxy_particle_t *particle, bool initial)
{
    particle->radius = initial ? 0.12f + galaxy_random_float() * 0.94f
                               : 1.02f + galaxy_random_float() * 0.16f;
    particle->angle = galaxy_random_float() * GALAXY_TAU;
    particle->inward = 0.045f + galaxy_random_float() * 0.075f;
    particle->spin = 0.55f + galaxy_random_float() * 1.35f;
    particle->size = 1.5f + galaxy_random_float() * 2.1f;
    particle->twinkle = galaxy_random_float() * GALAXY_TAU;
    particle->wobble = galaxy_random_float() * GALAXY_TAU;
    float layer_roll = galaxy_random_float();
    particle->layer = layer_roll < 0.46f ? GALAXY_FAR
                    : (layer_roll < 0.78f ? GALAXY_MID : GALAXY_NEAR);
    if (particle->layer == GALAXY_FAR) {
        particle->color_index = (uint8_t)(galaxy_random() % 2u);
    } else if (particle->layer == GALAXY_MID) {
        particle->color_index = (uint8_t)(2u + galaxy_random() % 2u);
    } else {
        particle->color_index = (uint8_t)(3u + galaxy_random() % 3u);
    }
}
static void galaxy_create(lv_obj_t *stage)
{
    s_root = lv_obj_create(stage);
    lv_obj_remove_style_all(s_root);
    lv_obj_set_size(s_root, 240, 240);
    lv_obj_set_pos(s_root, 0, 0);
    lv_obj_remove_flag(s_root, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_root, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_root, 0, 0);
    lv_obj_set_style_pad_all(s_root, 0, 0);

    s_disk_outer = galaxy_make_disk(s_root, 196, 0x184D59, 1, 220, 520, 100);
    lv_obj_set_style_transform_scale_y(s_disk_outer, 92, 0);
    s_disk_inner = galaxy_make_disk(s_root, 138, 0x22B8CE, 1, 20, 150, 92);
    lv_obj_set_style_transform_scale_y(s_disk_inner, 110, 0);

    for (int i = 0; i < GALAXY_PARTICLE_COUNT; i++) {
        galaxy_particle_t *p = &s_particles[i];
        p->obj = lv_obj_create(s_root);
        lv_obj_remove_style_all(p->obj);
        lv_obj_set_size(p->obj, 3, 3);
        lv_obj_remove_flag(p->obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(p->obj, LV_OBJ_FLAG_IGNORE_LAYOUT);
        lv_obj_set_style_radius(p->obj, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(p->obj, 0, 0);
        lv_obj_set_style_pad_all(p->obj, 0, 0);
        lv_obj_set_style_transform_pivot_x(p->obj, 1, 0);
        lv_obj_set_style_transform_pivot_y(p->obj, 1, 0);
        lv_obj_set_style_bg_opa(p->obj, 120, 0);
        lv_obj_set_style_bg_color(p->obj, lv_color_hex(s_colors[0]), 0);
        galaxy_reseed_particle(p, true);
    }

    /* Behind every designed ring/text child, above the stage gradient. */
    lv_obj_move_to_index(s_root, 0);
    ESP_LOGI(TAG, "accretion field created: %d particles", GALAXY_PARTICLE_COUNT);
}
static void galaxy_update_arcs(float dt)
{
    if (!galaxy_root_valid()) return;
    s_disk_angle_outer += dt * (8.0f + 11.0f * s_speed);
    s_disk_angle_inner -= dt * (6.0f + 8.0f * s_spin);
    if (s_disk_angle_outer > 360.0f) s_disk_angle_outer -= 360.0f;
    if (s_disk_angle_inner < -360.0f) s_disk_angle_inner += 360.0f;
    lv_arc_set_rotation(s_disk_outer, (int32_t)s_disk_angle_outer);
    lv_arc_set_rotation(s_disk_inner, (int32_t)s_disk_angle_inner + 360);
}

static void galaxy_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (!galaxy_root_valid() || s_mode == GALAXY_MODE_OFF) return;

    uint32_t now = lv_tick_get();
    static uint32_t last_ms;
    if (last_ms == 0) last_ms = now;
    float dt = (float)(now - last_ms) / 1000.0f;
    last_ms = now;
    if (dt <= 0.0f) return;
    if (dt > 0.12f) dt = 0.12f;
    s_time_acc += dt;

    float target_speed = 0.0f, target_spin = 0.0f;
    float target_brightness = 0.0f, target_radius = 88.0f;
    if (s_mode == GALAXY_MODE_HOME) {
        target_speed = 0.62f; target_spin = 0.55f;
        target_brightness = 0.72f; target_radius = 102.0f;
    } else if (s_mode == GALAXY_MODE_DETECTION) {
        target_speed = 2.85f; target_spin = 1.75f;
        target_brightness = 1.0f; target_radius = 96.0f;
    } else if (s_mode == GALAXY_MODE_RESULT) {
        target_speed = 0.38f; target_spin = 0.38f;
        target_brightness = 0.54f; target_radius = 88.0f;
    }
    float ease = dt * 2.8f;
    if (ease > 1.0f) ease = 1.0f;
    s_speed += (target_speed - s_speed) * ease;
    s_spin += (target_spin - s_spin) * ease;
    s_brightness += (target_brightness - s_brightness) * ease;
    s_max_radius += (target_radius - s_max_radius) * ease;

    galaxy_update_arcs(dt);
    lv_obj_set_style_arc_opa(s_disk_outer, (lv_opa_t)(45.0f + 70.0f * s_brightness), LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(s_disk_inner, (lv_opa_t)(48.0f + 58.0f * s_brightness), LV_PART_INDICATOR);
    for (int i = 0; i < GALAXY_PARTICLE_COUNT; i++) {
        galaxy_particle_t *p = &s_particles[i];
        if (p->obj == NULL || !lv_obj_is_valid(p->obj)) continue;

        p->radius -= p->inward * s_speed * dt * (0.60f + (1.0f - p->radius) * 1.75f);
        if (p->radius <= 0.035f) {
            galaxy_reseed_particle(p, false);
            continue;
        }
        p->angle += p->spin * s_spin * dt * (0.80f + (1.0f - p->radius) * 1.45f);
        if (p->angle > GALAXY_TAU) p->angle -= GALAXY_TAU;

        float radius_px = 9.0f + p->radius * s_max_radius;
        float depth = 0.32f + (1.0f - p->radius) * 0.68f;
        float tilt = 0.43f + 0.035f * sinf(s_time_acc * 1.4f + p->wobble);
        float x = (float)s_cx + cosf(p->angle) * radius_px;
        float y = (float)s_cy + sinf(p->angle) * radius_px * tilt;
        float shimmer = 0.86f + 0.14f * sinf(s_time_acc * 4.2f + p->twinkle);
        float scale = (0.48f + depth * 0.95f) * shimmer;
        int base = (int)(p->size + 0.5f);
        int px = (int)(x + 0.5f) - base / 2;
        int py = (int)(y + 0.5f) - base / 2;
        int opa = (int)(24.0f + 190.0f * depth * shimmer * s_brightness);
        if (opa > 235) opa = 235;

        lv_obj_set_pos(p->obj, px, py);
        lv_obj_set_style_bg_color(p->obj, lv_color_hex(s_colors[p->color_index]), 0);
        lv_obj_set_style_bg_opa(p->obj, (lv_opa_t)opa, 0);
        lv_obj_set_style_transform_scale_x(p->obj, (int32_t)(256.0f * scale * 1.15f), 0);
        lv_obj_set_style_transform_scale_y(p->obj, (int32_t)(256.0f * scale * 0.72f), 0);
        if (p->layer == GALAXY_NEAR) {
            lv_obj_set_style_transform_rotation(p->obj,
                (int32_t)((p->angle + GALAXY_PI * 0.5f) * (180.0f / GALAXY_PI) * 10.0f), 0);
        }
    }
}
void galaxy_effect_init(void)
{
    if (s_timer != NULL) return;
    s_timer = lv_timer_create(galaxy_timer_cb, GALAXY_TIMER_MS, NULL);
    if (s_timer == NULL) ESP_LOGE(TAG, "timer allocation failed");
}

void galaxy_effect_attach(lv_obj_t *stage, galaxy_mode_t mode, int center_x, int center_y)
{
    s_cx = center_x;
    s_cy = center_y;
    s_mode = mode;

    if (mode == GALAXY_MODE_OFF || stage == NULL) {
        if (galaxy_root_valid()) lv_obj_add_flag(s_root, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    if (!galaxy_root_valid()) {
        galaxy_clear_references();
        galaxy_create(stage);
    } else {
        if (lv_obj_get_parent(s_root) != stage) lv_obj_set_parent(s_root, stage);
        lv_obj_set_pos(s_root, 0, 0);
        lv_obj_move_to_index(s_root, 0);
        lv_obj_remove_flag(s_root, LV_OBJ_FLAG_HIDDEN);
    }

    if (s_disk_outer != NULL && lv_obj_is_valid(s_disk_outer)) {
        lv_obj_set_pos(s_disk_outer, s_cx - 98, s_cy - 98);
    }
    if (s_disk_inner != NULL && lv_obj_is_valid(s_disk_inner)) {
        lv_obj_set_pos(s_disk_inner, s_cx - 69, s_cy - 69);
    }
    ESP_LOGI(TAG, "scene=%d center=(%d,%d)", (int)mode, s_cx, s_cy);
}
