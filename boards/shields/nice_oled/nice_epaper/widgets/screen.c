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
#include <zmk/display/widgets/battery_status.h>
#include <zmk/display/widgets/output_status.h>


#include "layer.h"
#include "profile.h"
#include "screen.h"
#include "wpm.h"



static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

static struct zmk_widget_battery_status battery_status_widget;
static struct zmk_widget_output_status output_status_widget;


/**
 * luna
 **/

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_WPM)
#include "luna.h"
static struct zmk_widget_luna luna_widget;
#endif

/**
 * modifiers
 **/
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS)
#include "modifiers.h"                               // Incluir el archivo de cabecera de modifiers
static struct zmk_widget_modifiers modifiers_widget; // Declarar el widget de modifiers
#endif

/**
 * hid indicators
 **/

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_HID_INDICATORS)
#include "hid_indicators.h"
static struct zmk_widget_hid_indicators hid_indicators_widget;
#endif

/**
 * Draw canvas
 **/

static void draw_canvas(lv_obj_t *widget, lv_color_t cbuf[], const struct status_state *state) {
    lv_obj_t *canvas = lv_obj_get_child(widget, 0);

    // Draw widgets
    draw_background(canvas);
    // change the position
    draw_wpm_status(canvas, state);
    draw_profile_status(canvas, state);
    draw_layer_status(canvas, state);

    // Rotate for horizontal display
    rotate_canvas(canvas, cbuf);
}

/**
 * Battery status
 **/

// static void set_battery_status(struct zmk_widget_screen *widget,
//                                struct battery_status_state state) {
// #if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
//     widget->state.charging = state.usb_present;
// #endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */

//     widget->state.battery = state.level;

//     draw_canvas(widget->obj, widget->cbuf, &widget->state);
// }

// static void battery_status_update_cb(struct battery_status_state state) {
//     struct zmk_widget_screen *widget;
//     SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_battery_status(widget, state); }
// }

// static struct battery_status_state battery_status_get_state(const zmk_event_t *eh) {
//     const struct zmk_battery_state_changed *ev = as_zmk_battery_state_changed(eh);

//     return (struct battery_status_state){
//         .level = (ev != NULL) ? ev->state_of_charge : zmk_battery_state_of_charge(),
// #if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
//         .usb_present = zmk_usb_is_powered(),
// #endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */
//     };
// }

/**
 * Layer status
 **/

static void set_layer_status(struct zmk_widget_screen *widget, struct layer_status_state state) {
    widget->state.layer_index = state.index;
    widget->state.layer_label = state.label;

    draw_canvas(widget->obj, widget->cbuf, &widget->state);
}

static void layer_status_update_cb(struct layer_status_state state) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_layer_status(widget, state); }
}

static struct layer_status_state layer_status_get_state(const zmk_event_t *eh) {
    uint8_t index = zmk_keymap_highest_layer_active();
    return (struct layer_status_state){.index = index, .label = zmk_keymap_layer_name(index)};
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_layer_status, struct layer_status_state, layer_status_update_cb,
                            layer_status_get_state)

ZMK_SUBSCRIPTION(widget_layer_status, zmk_layer_state_changed);

#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_output_status, zmk_usb_conn_state_changed);
#endif
#if defined(CONFIG_ZMK_BLE)
ZMK_SUBSCRIPTION(widget_output_status, zmk_ble_active_profile_changed);
#endif

/**
 * WPM status
 **/

static void set_wpm_status(struct zmk_widget_screen *widget, struct wpm_status_state state) {
    for (int i = 0; i < 9; i++) {
        widget->state.wpm[i] = widget->state.wpm[i + 1];
    }
    widget->state.wpm[9] = state.wpm;

    draw_canvas(widget->obj, widget->cbuf, &widget->state);
}

static void wpm_status_update_cb(struct wpm_status_state state) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_wpm_status(widget, state); }
}

struct wpm_status_state wpm_status_get_state(const zmk_event_t *eh) {
    return (struct wpm_status_state){.wpm = zmk_wpm_get_state()};
};

ZMK_DISPLAY_WIDGET_LISTENER(widget_wpm_status, struct wpm_status_state, wpm_status_update_cb,
                            wpm_status_get_state)
ZMK_SUBSCRIPTION(widget_wpm_status, zmk_wpm_state_changed);

/**
 * Initialization
 **/

int zmk_widget_screen_init(struct zmk_widget_screen *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, CANVAS_HEIGHT, CANVAS_WIDTH);

    lv_obj_t *canvas = lv_canvas_create(widget->obj);
    lv_obj_align(canvas, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_canvas_set_buffer(canvas, widget->cbuf, CANVAS_HEIGHT, CANVAS_HEIGHT, LV_IMG_CF_TRUE_COLOR);

    sys_slist_append(&widgets, &widget->node);
    widget_layer_status_init();
    widget_wpm_status_init();

    zmk_widget_battery_status_init(&battery_status_widget, canvas);
    lv_obj_align(zmk_widget_battery_status_obj(&battery_status_widget), LV_ALIGN_TOP_RIGHT, -4, 2);

    zmk_widget_output_status_init(&output_status_widget, canvas);
    lv_obj_align(zmk_widget_output_status_obj(&output_status_widget), LV_ALIGN_TOP_RIGHT, -28, 2);

    #if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_WPM)
        zmk_widget_luna_init(&luna_widget, canvas);

        // ori: lv_obj_align(zmk_widget_luna_obj(&luna_widget), LV_ALIGN_TOP_LEFT, 36, 0);
        lv_obj_align(zmk_widget_luna_obj(&luna_widget), LV_ALIGN_TOP_LEFT, 100, 15);
    #endif

    #if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_HID_INDICATORS)
        zmk_widget_hid_indicators_init(&hid_indicators_widget, canvas);
    #endif

    #if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS)
        zmk_widget_modifiers_init(&modifiers_widget, canvas); // Inicializar el widget de modifiers
    #endif
        return 0;
}

lv_obj_t *zmk_widget_screen_obj(struct zmk_widget_screen *widget) { return widget->obj; }
