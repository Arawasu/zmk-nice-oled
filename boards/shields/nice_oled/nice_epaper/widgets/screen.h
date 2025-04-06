#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>
#include "util.h"

struct zmk_widget_screen {
    sys_snode_t node;
    lv_obj_t *obj;
    lv_color_t cbuf[CANVAS_HEIGHT * CANVAS_HEIGHT];
    struct status_state state;
};

int zmk_widget_screen_init(struct zmk_widget_screen *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_screen_obj(struct zmk_widget_screen *widget);

struct status_state {
    uint8_t battery;
    bool charging;

    struct zmk_endpoint_instance selected_endpoint;
    int active_profile_index;
    bool active_profile_connected;
    bool active_profile_bonded;

    const char *layer_label;
    uint8_t layer_index;

    uint8_t wpm[10];
};
