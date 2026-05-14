#include <zephyr/kernel.h>
#include <lvgl.h>
#include <zmk/display.h>

lv_obj_t *zmk_display_status_screen(void)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(screen, 0, LV_PART_MAIN);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    /* Single static blob — testing child object creation */
    lv_obj_t *blob = lv_obj_create(screen);
    if (blob) {
        lv_obj_set_style_radius(blob, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_bg_color(blob, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(blob, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(blob, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(blob, 0, LV_PART_MAIN);
        lv_obj_clear_flag(blob, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(blob, 16, 16);
        lv_obj_set_pos(blob, 56, 8);
    }

    return screen;
}
