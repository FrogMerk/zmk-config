#include <zephyr/kernel.h>
#include <zephyr/random/random.h>
#include <lvgl.h>
#include <zmk/display.h>
#include <zmk/events/position_state_changed.h>

#define NUM_BLOBS           5
#define DISP_W              32
#define DISP_H              128
#define BLOB_R_MIN          4
#define BLOB_R_MAX          9

/* Activity: 0-100, bumped on each keypress, decays over time */
#define ACTIVITY_PER_PRESS  25
#define ACTIVITY_DECAY_RATE 5   /* ticks between each -1 decay step */

/* Rise speed in fixed-point x16 px/tick */
#define SPEED_MIN_FP        8   /* 0.5 px/tick at idle */
#define SPEED_RANGE_FP      48  /* +3 px/tick at full activity */

struct blob {
    lv_obj_t *obj;
    int16_t   cx;
    int16_t   cy_fp;
    int16_t   r;
    int16_t   speed_fp;
};

static struct blob blobs[NUM_BLOBS];
static lv_timer_t *anim_timer;
static uint8_t     activity;
static uint8_t     decay_counter;

static int16_t blob_speed(void)
{
    int32_t s = SPEED_MIN_FP + (int32_t)activity * SPEED_RANGE_FP / 100;
    s = s * (int32_t)(12 + (sys_rand32_get() % 8)) / 16;
    return (int16_t)s;
}

static void place_blob(struct blob *b)
{
    lv_obj_set_size(b->obj, 2 * b->r, 2 * b->r);
    lv_obj_set_pos(b->obj, b->cx - b->r, (b->cy_fp >> 4) - b->r);
}

static void init_blob(struct blob *b, int index)
{
    b->r        = BLOB_R_MIN + (int16_t)(sys_rand32_get() % (BLOB_R_MAX - BLOB_R_MIN + 1));
    b->cx       = (int16_t)(b->r + sys_rand32_get() % (DISP_W - 2 * b->r));
    int16_t y   = (int16_t)(DISP_H * index / NUM_BLOBS + b->r);
    b->cy_fp    = (int16_t)(y << 4);
    b->speed_fp = blob_speed();
    place_blob(b);
}

static void anim_tick(lv_timer_t *t)
{
    ARG_UNUSED(t);

    if (activity > 0) {
        decay_counter++;
        if (decay_counter >= ACTIVITY_DECAY_RATE) {
            decay_counter = 0;
            activity--;
            for (int i = 0; i < NUM_BLOBS; i++) {
                blobs[i].speed_fp = blob_speed();
            }
        }
    }

    for (int i = 0; i < NUM_BLOBS; i++) {
        struct blob *b = &blobs[i];
        b->cy_fp -= b->speed_fp;

        if ((b->cy_fp >> 4) < -b->r) {
            b->r        = BLOB_R_MIN + (int16_t)(sys_rand32_get() % (BLOB_R_MAX - BLOB_R_MIN + 1));
            b->cx       = (int16_t)(b->r + sys_rand32_get() % (DISP_W - 2 * b->r));
            b->cy_fp    = (int16_t)((DISP_H + b->r) << 4);
            b->speed_fp = blob_speed();
        }

        place_blob(b);
    }
}

/* --- Keypress event wiring --------------------------------------------- */

struct lava_key_state { bool pressed; };

static void lava_key_update_cb(struct lava_key_state state)
{
    if (!state.pressed) {
        return;
    }
    uint16_t next = (uint16_t)activity + ACTIVITY_PER_PRESS;
    activity = (uint8_t)(next > 100 ? 100 : next);
    decay_counter = 0;
    for (int i = 0; i < NUM_BLOBS; i++) {
        blobs[i].speed_fp = blob_speed();
    }
}

static struct lava_key_state lava_key_get_state(const zmk_event_t *eh)
{
    const struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);
    return (struct lava_key_state){.pressed = ev ? ev->state : false};
}

ZMK_DISPLAY_WIDGET_LISTENER(lava_key, struct lava_key_state,
                            lava_key_update_cb, lava_key_get_state)
ZMK_SUBSCRIPTION(lava_key, zmk_position_state_changed)

/* --- Entry point -------------------------------------------------------- */

lv_obj_t *zmk_display_status_screen(void)
{
    lv_disp_set_rotation(lv_disp_get_default(), LV_DISP_ROT_90);

    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(screen, 0, LV_PART_MAIN);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < NUM_BLOBS; i++) {
        blobs[i].obj = lv_obj_create(screen);
        lv_obj_set_style_radius(blobs[i].obj, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_bg_color(blobs[i].obj, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(blobs[i].obj, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(blobs[i].obj, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(blobs[i].obj, 0, LV_PART_MAIN);
        lv_obj_clear_flag(blobs[i].obj, LV_OBJ_FLAG_SCROLLABLE);
        init_blob(&blobs[i], i);
    }

    anim_timer = lv_timer_create(anim_tick, CONFIG_ZMK_DISPLAY_TICK_PERIOD_MS, NULL);

    return screen;
}
