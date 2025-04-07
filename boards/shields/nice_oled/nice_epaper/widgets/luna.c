/*
 * luna.c (merged logic for WPM + Modifiers)
 */

 #include <zephyr/kernel.h>
 #include <zephyr/logging/log.h>
 LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);
 
 #include <zmk/event_manager.h>
 #include <zmk/events/wpm_state_changed.h>
 #include <zmk/events/keycode_state_changed.h>
 #include <zmk/wpm.h>
 #include <zmk/hid.h>
 #include <dt-bindings/zmk/modifiers.h>
 
 #include <zmk/display.h>
 #include <lvgl.h>
 
 #include "luna.h"
 
 // Declare the dog images
 LV_IMG_DECLARE(dog_sit1_90);
 LV_IMG_DECLARE(dog_sit2_90);
 LV_IMG_DECLARE(dog_walk1_90);
 LV_IMG_DECLARE(dog_walk2_90);
 LV_IMG_DECLARE(dog_run1_90);
 LV_IMG_DECLARE(dog_run2_90);
 LV_IMG_DECLARE(dog_sneak1_90);
 LV_IMG_DECLARE(dog_sneak2_90);
 
 // WPM-based frames
 static const lv_img_dsc_t *idle_imgs[]  = { &dog_sit1_90,   &dog_sit2_90 };
 static const lv_img_dsc_t *slow_imgs[]  = { &dog_walk1_90,  &dog_walk2_90 };
 static const lv_img_dsc_t *mid_imgs[]   = { &dog_walk1_90,  &dog_walk2_90 }; // Could be distinct
 static const lv_img_dsc_t *fast_imgs[]  = { &dog_run1_90,   &dog_run2_90 };
 
 // “modifier override” frames
 static const lv_img_dsc_t *mod_sit[]    = { &dog_sit1_90,   &dog_sit2_90 };
 static const lv_img_dsc_t *mod_walk[]   = { &dog_walk1_90,  &dog_walk2_90 };
 static const lv_img_dsc_t *mod_run[]    = { &dog_run1_90,   &dog_run2_90 };
 static const lv_img_dsc_t *mod_sneak[]  = { &dog_sneak1_90, &dog_sneak2_90 };
 
 // For readability
 #define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))
 #define SRC(arr)        ((const void **)(arr)), ARRAY_SIZE(arr)
 
 // Track our overall state
 struct luna_state {
     uint8_t wpm;
     uint8_t mods;
 };
 
 enum anim_state {
     ANIM_NONE,
     ANIM_IDLE,
     ANIM_SLOW,
     ANIM_MID,
     ANIM_FAST,
     ANIM_OVERRIDE  // if modifiers pressed
 };
 
 static enum anim_state current_anim_state = ANIM_NONE;
 
 // The widget list
 static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);
 
 // Helper to choose an override if any mods are pressed
 static const lv_img_dsc_t **get_modifier_frames(uint8_t mods, size_t *count_out) {
     // This logic tries a priority: SHIFT → SNEAK, CTRL → RUN, ALT → WALK, GUI → SIT
     // or you could change the order if you want a different priority.
     if (mods & (MOD_LSFT | MOD_RSFT)) {
         *count_out = ARRAY_SIZE(mod_sneak);
         return mod_sneak;
     } else if (mods & (MOD_LCTL | MOD_RCTL)) {
         *count_out = ARRAY_SIZE(mod_run);
         return mod_run;
     } else if (mods & (MOD_LALT | MOD_RALT)) {
         *count_out = ARRAY_SIZE(mod_walk);
         return mod_walk;
     } else if (mods & (MOD_LGUI | MOD_RGUI)) {
         *count_out = ARRAY_SIZE(mod_sit);
         return mod_sit;
     }
     return NULL;
 }
 
 // Set the animations on the single Luna animimg
 static void set_animation(lv_obj_t *animimg, struct luna_state s) {
     // Check for modifiers first:
     size_t frames_count = 0;
     const lv_img_dsc_t **frames = get_modifier_frames(s.mods, &frames_count);
     if (frames) {
         // A modifier is pressed => override WPM-based animations
         if (current_anim_state != ANIM_OVERRIDE) {
             lv_animimg_set_src(animimg, (const void **)frames, frames_count);
             // You can pick a suitable speed:
             lv_animimg_set_duration(animimg, 200); // e.g. 200 ms
             lv_animimg_set_repeat_count(animimg, LV_ANIM_REPEAT_INFINITE);
             lv_animimg_start(animimg);
             current_anim_state = ANIM_OVERRIDE;
         }
         return;
     }
 
     // Otherwise, no modifiers => pick WPM-based frames
     if (s.wpm < 15) {
         if (current_anim_state != ANIM_IDLE) {
             lv_animimg_set_src(animimg, SRC(idle_imgs));
             lv_animimg_set_duration(animimg, 960);
             lv_animimg_set_repeat_count(animimg, LV_ANIM_REPEAT_INFINITE);
             lv_animimg_start(animimg);
             current_anim_state = ANIM_IDLE;
         }
     } else if (s.wpm < 30) {
         if (current_anim_state != ANIM_SLOW) {
             lv_animimg_set_src(animimg, SRC(slow_imgs));
             lv_animimg_set_duration(animimg, 200);
             lv_animimg_set_repeat_count(animimg, LV_ANIM_REPEAT_INFINITE);
             lv_animimg_start(animimg);
             current_anim_state = ANIM_SLOW;
         }
     } else if (s.wpm < 70) {
         if (current_anim_state != ANIM_MID) {
             lv_animimg_set_src(animimg, SRC(mid_imgs));
             lv_animimg_set_duration(animimg, 200);
             lv_animimg_set_repeat_count(animimg, LV_ANIM_REPEAT_INFINITE);
             lv_animimg_start(animimg);
             current_anim_state = ANIM_MID;
         }
     } else {
         if (current_anim_state != ANIM_FAST) {
             lv_animimg_set_src(animimg, SRC(fast_imgs));
             lv_animimg_set_duration(animimg, 200);
             lv_animimg_set_repeat_count(animimg, LV_ANIM_REPEAT_INFINITE);
             lv_animimg_start(animimg);
             current_anim_state = ANIM_FAST;
         }
     }
 }
 
 // This is our “get current Luna state” function. We always fetch both wpm & mods.
 static struct luna_state get_luna_state(const zmk_event_t *eh) {
     struct luna_state s = {
         .wpm  = zmk_wpm_get_state(),
         .mods = zmk_hid_get_explicit_mods(),
     };
     return s;
 }
 
 // On each event, update all the Luna widgets
 static void luna_update_cb(struct luna_state s) {
     struct zmk_widget_luna *widget;
     SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
         set_animation(widget->obj, s);
     }
 }
 
 // Register to both wpm & keycode changes
 ZMK_DISPLAY_WIDGET_LISTENER(widget_luna, struct luna_state, luna_update_cb, get_luna_state);
 ZMK_SUBSCRIPTION(widget_luna, zmk_wpm_state_changed);
 ZMK_SUBSCRIPTION(widget_luna, zmk_keycode_state_changed);
 
 // Standard widget init
 int zmk_widget_luna_init(struct zmk_widget_luna *widget, lv_obj_t *parent) {
     widget->obj = lv_animimg_create(parent);
     lv_obj_align(widget->obj, LV_ALIGN_TOP_LEFT, 36, 0);
     // Or pick some other default position as you like
 
     sys_slist_append(&widgets, &widget->node);
 
     // Kick off the ZMK event listener so that it’s ready
     widget_luna_init();
     return 0;
 }
 
 lv_obj_t *zmk_widget_luna_obj(struct zmk_widget_luna *widget) {
     return widget->obj;
 }
 