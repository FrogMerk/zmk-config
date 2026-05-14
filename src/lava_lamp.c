#include <zephyr/kernel.h>
#include <zephyr/random/random.h>
#include <lvgl.h>
#include <zmk/display.h>
#include <zmk/events/wpm_state_changed.h>
#include <zmk/wpm.h>

#define NUM_BLOBS     5
#define DISP_W        32
#define DISP_H        128
#define BLOB_R_MIN    4
#define BLOB_R_MAX    9

/* Rise speeds stored as fixed-point (x16) to avoid floats on the M4 */
#define SPEED_BASE    13   /* ~0.8 px/tick at idle */
#define SPEED_PER_WPM 4    /* +0.25 px/tick per WPM unit */
#define SPEED_MAX     64   /* 4 px/tick hard ceiling */

struct blob {
    lv_obj_t *obj;
    int16_t   cx;
    int16_t   cy_fp;    /* centre-y, fixed-point (<<4) */
    int16_t   r;
    int16_t   speed_fp;
};

static struct blob blobs[NUM_BLOBS];
static uint8_t     current_wpm;
static lv_timer_t *anim_timer;

static int16_t blob_speed(void)
{
    int32_t s = SPEED_BASE + (int32_t)current_wpm * SPEED_PER_WPM;
    if (s > SPEED_MAX) {
        s = SPEED_MAX;
    }
    /* ±25 % jitter so blobs don't all move in lockstep */
    s = s * (int32_t)(12 + (sys_rand32_get() % 8)) / 16;
    return (int16_t)s;
}

static void place_blob(struct blob *b)
{
    lv_obj_set_size(b->obj, 2 * b->r, 2 * b->r);
    lv_obj_set_pos(b->obj, b->cx - b->r, (b->cy_fp >> 4) - b->r);
}

static void reset_blob(struct blob *b, int index, bool stagger)
{
    b->r  = BLOB_R_MIN + (int16_t)(sys_rand32_get() % (BLOB_R_MAX - BLOB_R_MIN + 1));
    b->cx = (int16_t)(b->r + sys_rand32_get() % (DISP_W - 2 * b->r));

    if (stagger) {
        /* spread blobs evenly down the screen on first load */
        int16_t y = (int16_t)(DISP_H * index / NUM_BLOBS + b->r);
        b->cy_fp = (int16_t)(y << 4);
    } else {
        b->cy_fp = (int16_t)((DISP_H + b->r) << 4);
    }

    b->speed_fp = blob_speed();
    place_blob(b);
}

static void anim_tick(lv_timer_t *t)
{
    ARG_UNUSED(t);
    for (int i = 0; i < NUM_BLOBS; i++) {
        struct blob *b = &blobs[i];
        b->cy_fp -= b->speed_fp;

        if ((b->cy_fp >> 4) < -b->r) {
            /* blob exited top: give it a fresh size, x, and speed */
            b->r        = BLOB_R_MIN + (int16_t)(sys_rand32_get() % (BLOB_R_MAX - BLOB_R_MIN + 1));
            b->cx       = (int16_t)(b->r + sys_rand32_get() % (DISP_W - 2 * b->r));
            b->cy_fp    = (int16_t)((DISP_H + b->r) << 4);
            b->speed_fp = blob_speed();
        }

        place_blob(b);
    }
}

/* --- WPM event wiring -------------------------------------------------- */

struct lava_wpm_state { uint8_t wpm; };

static void lava_wpm_update_cb(struct lava_wpm_state state)
{
    current_wpm = state.wpm;
    for (int i = 0; i < NUM_BLOBS; i++) {
        blobs[i].speed_fp = blob_speed();
    }
}

static struct lava_wpm_state lava_wpm_get_state(const zmk_event_t *eh)
{
    const struct zmk_wpm_state_changed *ev = as_zmk_wpm_state_changed(eh);
    return (struct lava_wpm_state){.wpm = ev ? ev->state : zmk_wpm_get_state()};
}

ZMK_DISPLAY_WIDGET_LISTENER(lava_wpm, struct lava_wpm_state,
                            lava_wpm_update_cb, lava_wpm_get_state)
ZMK_SUBSCRIPTION(lava_wpm, zmk_wpm_state_changed)

/* --- Entry point -------------------------------------------------------- */

lv_obj_t *zmk_display_status_screen(void)
{
    /* rotate LVGL logical canvas to match portrait mounting */
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
        reset_blob(&blobs[i], i, true);
    }

    anim_timer = lv_timer_create(anim_tick, CONFIG_ZMK_DISPLAY_TICK_PERIOD_MS, NULL);

    return screen;
}
