#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/events/wpm_state_changed.h>
#include <zmk/battery.h>
#include <zmk/ble.h>
#include <zmk/display.h>
#include <zmk/endpoints.h>
#include <zmk/keymap.h>
#include <zmk/usb.h>
#include <zmk/wpm.h>

#include "screen.h"
#include "layer.h"
#include "profile.h"
#include "wpm.h"

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_WPM)
#include "luna.h"
static struct zmk_widget_luna luna_widget;
#endif

// #if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS)
// #include "modifiers.h"
// static struct zmk_widget_modifiers modifiers_widget;
// #endif

// #if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_HID_INDICATORS)
// #include "hid_indicators.h"
// static struct zmk_widget_hid_indicators hid_indicators_widget;
// #endif

extern const lv_img_dsc_t bolt;

/* ------------------------------------------------------------------------- */
/*                           STRUCTS & GLOBALS                                */
/* ------------------------------------------------------------------------- */

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

/* This struct tracks which endpoint is active, as well as BLE profile state */
struct output_status_state {
    struct zmk_endpoint_instance selected_endpoint;
    uint8_t active_profile_index;
    bool active_profile_connected;
    bool active_profile_bonded;
};

struct battery_status_state {
    uint8_t level;
    bool usb_present;
};

/* ------------------------------------------------------------------------- */
/*                               PROTOTYPES                                  */
/* ------------------------------------------------------------------------- */

static void draw_canvas(lv_obj_t *widget, lv_color_t cbuf[], const struct status_state *state);

/* ------------------------------------------------------------------------- */
/*                             BATTERY LOGIC                                 */
/* ------------------------------------------------------------------------- */

static struct battery_status_state battery_status_get_state(const zmk_event_t *eh) {
    return (struct battery_status_state){
        .level = zmk_battery_state_of_charge(),
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
        .usb_present = zmk_usb_is_powered(),
#else
        .usb_present = false,
#endif
    };
}

static void set_battery_status(struct zmk_widget_screen *widget, struct battery_status_state state) {
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
    widget->state.charging = state.usb_present;
#endif
    widget->state.battery = state.level;
    draw_canvas(widget->obj, widget->cbuf, &widget->state);
}

static void battery_status_update_cb(struct battery_status_state state) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        set_battery_status(widget, state);
    }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_battery_status, struct battery_status_state,
                            battery_status_update_cb, battery_status_get_state)

ZMK_SUBSCRIPTION(widget_battery_status, zmk_battery_state_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)

ZMK_SUBSCRIPTION(widget_battery_status, zmk_usb_conn_state_changed);
#endif

/* ------------------------------------------------------------------------- */
/*                          OUTPUT/PROFILE LOGIC                              */
/* ------------------------------------------------------------------------- */

static void set_output_status(struct zmk_widget_screen *widget,
                              const struct output_status_state *state) {
    widget->state.selected_endpoint      = state->selected_endpoint;
    widget->state.active_profile_index   = state->active_profile_index;
    widget->state.active_profile_connected = state->active_profile_connected;
    widget->state.active_profile_bonded  = state->active_profile_bonded;
    draw_canvas(widget->obj, widget->cbuf, &widget->state);
}

static void output_status_update_cb(struct output_status_state state) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        set_output_status(widget, &state);
    }
}

static struct output_status_state output_status_get_state(const zmk_event_t *eh) {
    return (struct output_status_state){
        .selected_endpoint         = zmk_endpoints_selected(),
        .active_profile_index      = zmk_ble_active_profile_index(),
        .active_profile_connected  = zmk_ble_active_profile_is_connected(),
        .active_profile_bonded     = !zmk_ble_active_profile_is_open(),
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_output_status, struct output_status_state,
    output_status_update_cb, output_status_get_state)

ZMK_SUBSCRIPTION(widget_output_status, zmk_endpoint_changed); // <-- 🔥 add this here
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_output_status, zmk_usb_conn_state_changed);
#endif
#if defined(CONFIG_ZMK_BLE)
ZMK_SUBSCRIPTION(widget_output_status, zmk_ble_active_profile_changed);
#endif


/* ------------------------------------------------------------------------- */
/*                             LAYER LOGIC                                   */
/* ------------------------------------------------------------------------- */

static void set_layer_status(struct zmk_widget_screen *widget, struct layer_status_state state) {
    widget->state.layer_index = state.index;
    widget->state.layer_label = state.label;
    draw_canvas(widget->obj, widget->cbuf, &widget->state);
}

static void layer_status_update_cb(struct layer_status_state state) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        set_layer_status(widget, state);
    }
}

static struct layer_status_state layer_status_get_state(const zmk_event_t *eh) {
    uint8_t index = zmk_keymap_highest_layer_active();
    return (struct layer_status_state){
        .index = index,
        .label = zmk_keymap_layer_name(index),
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_layer_status, struct layer_status_state,
                            layer_status_update_cb, layer_status_get_state)

ZMK_SUBSCRIPTION(widget_layer_status, zmk_layer_state_changed);

/* ------------------------------------------------------------------------- */
/*                             DRAW FUNCTIONS                                */
/* ------------------------------------------------------------------------- */

void draw_battery(lv_obj_t *canvas, const struct status_state *state) {
    lv_draw_rect_dsc_t rect_black_dsc;
    init_rect_dsc(&rect_black_dsc, LVGL_BACKGROUND);

    lv_draw_rect_dsc_t rect_white_dsc;
    init_rect_dsc(&rect_white_dsc, LVGL_FOREGROUND);

    lv_canvas_draw_rect(canvas, 0, 2, 29, 12, &rect_white_dsc); // outer
    lv_canvas_draw_rect(canvas, 1, 3, 27, 10, &rect_black_dsc); // inner
    lv_canvas_draw_rect(canvas, 2, 4, (state->battery + 2) / 4, 8, &rect_white_dsc); // fill
    lv_canvas_draw_rect(canvas, 30, 5, 3, 6, &rect_white_dsc); // nub
    lv_canvas_draw_rect(canvas, 31, 6, 1, 4, &rect_black_dsc); // inner nub

    if (state->charging) {
        lv_draw_img_dsc_t img_dsc;
        lv_draw_img_dsc_init(&img_dsc);
        lv_canvas_draw_img(canvas, 9, -1, &bolt, &img_dsc); // restore floaty bolt
    }
}


static void draw_canvas(lv_obj_t *widget, lv_color_t cbuf[], const struct status_state *state) {
    lv_obj_t *canvas = lv_obj_get_child(widget, 0);

    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &lv_font_montserrat_16, LV_TEXT_ALIGN_RIGHT);

    lv_draw_rect_dsc_t rect_black_dsc;
    init_rect_dsc(&rect_black_dsc, LVGL_BACKGROUND);

    // Clear screen
    lv_canvas_draw_rect(canvas, 0, 0, CANVAS_WIDTH, CANVAS_HEIGHT, &rect_black_dsc);

    // Draw battery
    draw_battery(canvas, state);

    // Output status icon
    char output_text[10] = {0};
    switch (state->selected_endpoint.transport) {
    case ZMK_TRANSPORT_USB:
        strcat(output_text, LV_SYMBOL_USB);
        break;
    case ZMK_TRANSPORT_BLE:
        if (state->active_profile_bonded) {
            strcat(output_text, state->active_profile_connected ? LV_SYMBOL_WIFI : LV_SYMBOL_CLOSE);
        } else {
            strcat(output_text, LV_SYMBOL_SETTINGS);
        }
        break;
    }
    lv_canvas_draw_text(canvas, 0, 0, 64, &label_dsc, output_text);

    // Draw other items
    draw_profile_status(canvas, state);
    draw_layer_status(canvas, state);

    // Rotate/transform if needed
    rotate_canvas(canvas, cbuf);
}

/* ------------------------------------------------------------------------- */
/*                           SCREEN INIT                                     */
/* ------------------------------------------------------------------------- */

int zmk_widget_screen_init(struct zmk_widget_screen *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, CANVAS_HEIGHT, CANVAS_WIDTH);

    lv_obj_t *canvas = lv_canvas_create(widget->obj);
    lv_obj_align(canvas, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_canvas_set_buffer(canvas, widget->cbuf, CANVAS_HEIGHT, CANVAS_HEIGHT, LV_IMG_CF_TRUE_COLOR);

    sys_slist_append(&widgets, &widget->node);

    // Initialize each sub-widget
    widget_layer_status_init();
    widget_output_status_init();
    widget_battery_status_init();

    // Force an immediate battery update
    battery_status_update_cb(battery_status_get_state(NULL));

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_WPM)
    zmk_widget_luna_init(&luna_widget, canvas);
    lv_obj_align(zmk_widget_luna_obj(&luna_widget), LV_ALIGN_TOP_MID, -30, 15);
    #endif

// #if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_HID_INDICATORS)
//     zmk_widget_hid_indicators_init(&hid_indicators_widget, canvas);
// #endif

// #if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS)
//     zmk_widget_modifiers_init(&modifiers_widget, canvas);
// #endif

    return 0;
}

lv_obj_t *zmk_widget_screen_obj(struct zmk_widget_screen *widget) {
    return widget->obj;
}
