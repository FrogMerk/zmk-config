#include <zephyr/kernel.h>
#include <zephyr/random/random.h>
#include <lvgl.h>
#include <zmk/display.h>

#define NUM_BLOBS     5
#define SCREEN_X_MAX  128
#define SCREEN_Y_MAX  32
#define BLOB_R_MIN    4
#define BLOB_R_MAX    9

/* Fixed-point x16. Each blob gets a slightly different speed on init/reset. */
#define SPEED_BASE_FP  20   /* ~1.25 px/tick */
#define SPEED_RANGE_FP 16   /* ±1 px/tick variation */

struct blob {
    lv_obj_t *obj;
    int16_t   cx_fp;
    int16_t   cy;
    int16_t   r;
    int16_t   speed_fp;
};

static struct blob  blobs[NUM_BLOBS];
static lv_style_t   blob_style;
static lv_timer_t  *anim_timer;

static int16_t rand_speed(void)
{
    return (int16_t)(SPEED_BASE_FP + (sys_rand32_get() % SPEED_RANGE_FP));
}

static void place_blob(struct blob *b)
{
    lv_obj_set_pos(b->obj, (b->cx_fp >> 4) - b->r, b->cy - b->r);
}

static void init_blob(struct blob *b, int index)
{
    b->r        = BLOB_R_MIN + (int16_t)(sys_rand32_get() % (BLOB_R_MAX - BLOB_R_MIN + 1));
    b->cy       = (int16_t)(b->r + sys_rand32_get() % (SCREEN_Y_MAX - 2 * b->r));
    int16_t x   = (int16_t)(SCREEN_X_MAX * index / NUM_BLOBS + b->r);
    b->cx_fp    = (int16_t)(x << 4);
    b->speed_fp = rand_speed();
    lv_obj_set_size(b->obj, 2 * b->r, 2 * b->r);
    place_blob(b);
}

static void anim_tick(lv_timer_t *t)
{
    ARG_UNUSED(t);
    for (int i = 0; i < NUM_BLOBS; i++) {
        struct blob *b = &blobs[i];
        b->cx_fp -= b->speed_fp;
        if ((b->cx_fp >> 4) < -b->r) {
            b->r        = BLOB_R_MIN + (int16_t)(sys_rand32_get() % (BLOB_R_MAX - BLOB_R_MIN + 1));
            b->cy       = (int16_t)(b->r + sys_rand32_get() % (SCREEN_Y_MAX - 2 * b->r));
            b->cx_fp    = (int16_t)((SCREEN_X_MAX + b->r) << 4);
            b->speed_fp = rand_speed();
            lv_obj_set_size(b->obj, 2 * b->r, 2 * b->r);
        }
        place_blob(b);
    }
}

lv_obj_t *zmk_display_status_screen(void)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(screen, 0, LV_PART_MAIN);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_style_init(&blob_style);
    lv_style_set_radius(&blob_style, LV_RADIUS_CIRCLE);
    lv_style_set_bg_color(&blob_style, lv_color_black());
    lv_style_set_bg_opa(&blob_style, LV_OPA_COVER);
    lv_style_set_border_width(&blob_style, 0);
    lv_style_set_pad_all(&blob_style, 0);

    for (int i = 0; i < NUM_BLOBS; i++) {
        blobs[i].obj = lv_obj_create(screen);
        if (!blobs[i].obj) {
            continue;
        }
        lv_obj_add_style(blobs[i].obj, &blob_style, 0);
        lv_obj_clear_flag(blobs[i].obj, LV_OBJ_FLAG_SCROLLABLE);
        init_blob(&blobs[i], i);
    }

    anim_timer = lv_timer_create(anim_tick, CONFIG_ZMK_DISPLAY_TICK_PERIOD_MS, NULL);

    return screen;
}
