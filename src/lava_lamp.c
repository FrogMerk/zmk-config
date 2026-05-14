#include <zephyr/kernel.h>
#include <zephyr/random/random.h>
#include <lvgl.h>
#include <zmk/display.h>
#include <zmk/events/position_state_changed.h>

#define NUM_BLOBS           5
#define SCREEN_X_MAX        128
#define SCREEN_Y_MAX        32
#define BLOB_R_MIN          4
#define BLOB_R_MAX          9

#define ACTIVITY_PER_PRESS  25
#define ACTIVITY_DECAY_RATE 5

/* Rise speed in fixed-point x16 px/tick along the x-axis */
#define SPEED_MIN_FP        8    /* 0.5 px/tick at idle */
#define SPEED_RANGE_FP      48   /* +3 px/tick at full activity */

struct blob {
    lv_obj_t *obj;
    int16_t   cx_fp;
    int16_t   cy;
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
    lv_obj_set_pos(b->obj, (b->cx_fp >> 4) - b->r, b->cy - b->r);
}

static void init_blob(struct blob *b, int index)
{
    b->r        = BLOB_R_MIN + (int16_t)(sys_rand32_get() % (BLOB_R_MAX - BLOB_R_MIN + 1));
    b->cy       = (int16_t)(b->r + sys_rand32_get() % (SCREEN_Y_MAX - 2 * b->r));
    int16_t x   = (int16_t)(SCREEN_X_MAX * index / NUM_BLOBS + b->r);
    b->cx_fp    = (int16_t)(x << 4);
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
        b->cx_fp -= b->speed_fp;

        if ((b->cx_fp >> 4) < -b->r) {
            b->r        = BLOB_R_MIN + (int16_t)(sys_rand32_get() % (BLOB_R_MAX - BLOB_R_MIN + 1));
            b->cy       = (int16_t)(b->r + sys_rand32_get() % (SCREEN_Y_MAX - 2 * b->r));
            b->cx_fp    = (int16_t)((SCREEN_X_MAX + b->r) << 4);
            b->speed_fp = blob_speed();
        }

        place_blob(b);
    }
}

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

lv_obj_t *zmk_display_status_screen(void)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(screen, 0, LV_PART_MAIN);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < NUM_BLOBS; i++) {
        blobs[i].obj = lv_obj_create(screen);
        if (!blobs[i].obj) {
            continue;
        }
        lv_obj_set_style_radius(blobs[i].obj, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_bg_color(blobs[i].obj, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(blobs[i].obj, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(blobs[i].obj, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(blobs[i].obj, 0, LV_PART_MAIN);
        lv_obj_clear_flag(blobs[i].obj, LV_OBJ_FLAG_SCROLLABLE);
        init_blob(&blobs[i], i);
    }

    anim_timer = lv_timer_create(anim_tick, CONFIG_ZMK_DISPLAY_TICK_PERIOD_MS, NULL);

    return screen;
}
