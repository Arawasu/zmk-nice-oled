/*
 * Combined single-luna with:
 *  - WPM-based idle/walk/run
 *  - Modifiers-based override (Shift = sneak, etc.)
 *  - HID lock-based bark (Caps/Num/Scroll)
 */

 #include <zephyr/kernel.h>
 #include <zephyr/logging/log.h>
 LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);
 
 #include <zmk/display.h>
 #include <zmk/event_manager.h>
 #include <zmk/events/wpm_state_changed.h>
 #include <zmk/events/keycode_state_changed.h>
 #include <zmk/events/hid_indicators_changed.h>
 #include <zmk/hid.h>
 #include <zmk/wpm.h>
 #include <dt-bindings/zmk/modifiers.h>
 
 #include <lvgl.h>
 #include "luna.h"
 
 // HID lock bits
 #define LED_NLCK  0x01
 #define LED_CLCK  0x02
 #define LED_SLCK  0x04
 
 // Dog images
 LV_IMG_DECLARE(dog_sit1_90);
 LV_IMG_DECLARE(dog_sit2_90);
 LV_IMG_DECLARE(dog_walk1_90);
 LV_IMG_DECLARE(dog_walk2_90);
 LV_IMG_DECLARE(dog_run1_90);
 LV_IMG_DECLARE(dog_run2_90);
 LV_IMG_DECLARE(dog_sneak1_90);
 LV_IMG_DECLARE(dog_sneak2_90);
 LV_IMG_DECLARE(dog_bark1_90);
 LV_IMG_DECLARE(dog_bark2_90);
 
 // WPM-based frames
 static const lv_img_dsc_t *idle_imgs[] =  { &dog_sit1_90,  &dog_sit2_90  };
 static const lv_img_dsc_t *slow_imgs[] =  { &dog_walk1_90, &dog_walk2_90 };
 static const lv_img_dsc_t *mid_imgs[]  =  { &dog_walk1_90, &dog_walk2_90 }; // Could be different
 static const lv_img_dsc_t *fast_imgs[] =  { &dog_run1_90,  &dog_run2_90  };
 
 // Mod override frames
 static const lv_img_dsc_t *mod_sit[]   =  { &dog_sit1_90,   &dog_sit2_90   };
 static const lv_img_dsc_t *mod_walk[]  =  { &dog_walk1_90,  &dog_walk2_90  };
 static const lv_img_dsc_t *mod_run[]   =  { &dog_run1_90,   &dog_run2_90   };
 static const lv_img_dsc_t *mod_sneak[] =  { &dog_sneak1_90, &dog_sneak2_90 };
 
 // HID locks -> bark
 static const lv_img_dsc_t *bark_imgs[] = { &dog_bark1_90,  &dog_bark2_90 };
 
 #define MY_ARRSZ(arr) (sizeof(arr) / sizeof(arr[0])) // Renamed to avoid redefinition warnings
 
 enum anim_state {
     ANIM_NONE,
     ANIM_IDLE,
     ANIM_SLOW,
     ANIM_MID,
     ANIM_FAST,
     ANIM_OVERRIDE  // e.g. bark or mod override
 };
 
 static enum anim_state current_anim_state = ANIM_NONE;
 static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);
 
 struct luna_state {
     uint8_t wpm;
     uint8_t mods;        // (Using explicit_mods to keep older ZMK builds happy)
     uint8_t indicators;  // Caps/Num/Scroll
 };
 
 static struct luna_state g_luna_state = {0, 0, 0};
 
 /* 
  * For SHIFT -> SNEAK, CTRL -> RUN, ALT -> WALK, GUI -> SIT
  * (Priority: SHIFT first, else CTRL, else ALT, else GUI)
  */
 static const lv_img_dsc_t **get_modifier_frames(uint8_t mods, size_t *count_out)
 {
     if (mods & (MOD_LSFT | MOD_RSFT)) {
         *count_out = MY_ARRSZ(mod_sneak);
         return mod_sneak;
     } else if (mods & (MOD_LCTL | MOD_RCTL)) {
         *count_out = MY_ARRSZ(mod_run);
         return mod_run;
     } else if (mods & (MOD_LALT | MOD_RALT)) {
         *count_out = MY_ARRSZ(mod_walk);
         return mod_walk;
     } else if (mods & (MOD_LGUI | MOD_RGUI)) {
         *count_out = MY_ARRSZ(mod_sit);
         return mod_sit;
     }
     return NULL; // No modifier override
 }
 
 /*
  * Main logic for deciding which frames to animate:
  *
  * 1) If any lock is active -> bark
  * 2) Else if any mod is pressed -> override
  * 3) Else pick WPM-based frames
  */
 static void set_animation(lv_obj_t *animimg, struct luna_state s)
 {
     /* Step 1: Caps/Num/Scroll = bark */
     if (s.indicators & (LED_CLCK | LED_NLCK | LED_SLCK)) {
         if (current_anim_state != ANIM_OVERRIDE) {
             lv_animimg_set_src(animimg, (const void **)bark_imgs, 2);
             lv_animimg_set_duration(animimg, 200);
             lv_animimg_set_repeat_count(animimg, LV_ANIM_REPEAT_INFINITE);
             lv_animimg_start(animimg);
             current_anim_state = ANIM_OVERRIDE;
         }
         return;
     }
 
     /* Step 2: Any normal modifier -> override */
     size_t frames_count = 0;
     const lv_img_dsc_t **frames = get_modifier_frames(s.mods, &frames_count);
     if (frames) {
         if (current_anim_state != ANIM_OVERRIDE) {
             lv_animimg_set_src(animimg, (const void **)frames, frames_count);
             lv_animimg_set_duration(animimg, 200);
             lv_animimg_set_repeat_count(animimg, LV_ANIM_REPEAT_INFINITE);
             lv_animimg_start(animimg);
             current_anim_state = ANIM_OVERRIDE;
         }
         return;
     }
 
     /* Step 3: WPM-based idle/walk/run */
     if (s.wpm < 15) {
         if (current_anim_state != ANIM_IDLE) {
             lv_animimg_set_src(animimg, (const void **)idle_imgs, MY_ARRSZ(idle_imgs));
             lv_animimg_set_duration(animimg, 960);
             lv_animimg_set_repeat_count(animimg, LV_ANIM_REPEAT_INFINITE);
             lv_animimg_start(animimg);
             current_anim_state = ANIM_IDLE;
         }
     } else if (s.wpm < 30) {
         if (current_anim_state != ANIM_SLOW) {
             lv_animimg_set_src(animimg, (const void **)slow_imgs, MY_ARRSZ(slow_imgs));
             lv_animimg_set_duration(animimg, 200);
             lv_animimg_set_repeat_count(animimg, LV_ANIM_REPEAT_INFINITE);
             lv_animimg_start(animimg);
             current_anim_state = ANIM_SLOW;
         }
     } else if (s.wpm < 70) {
         if (current_anim_state != ANIM_MID) {
             lv_animimg_set_src(animimg, (const void **)mid_imgs, MY_ARRSZ(mid_imgs));
             lv_animimg_set_duration(animimg, 200);
             lv_animimg_set_repeat_count(animimg, LV_ANIM_REPEAT_INFINITE);
             lv_animimg_start(animimg);
             current_anim_state = ANIM_MID;
         }
     } else {
         if (current_anim_state != ANIM_FAST) {
             lv_animimg_set_src(animimg, (const void **)fast_imgs, MY_ARRSZ(fast_imgs));
             lv_animimg_set_duration(animimg, 200);
             lv_animimg_set_repeat_count(animimg, LV_ANIM_REPEAT_INFINITE);
             lv_animimg_start(animimg);
             current_anim_state = ANIM_FAST;
         }
     }
 }
 
 /*
  * Single aggregator. 
  * We'll fetch WPM, "explicit" mods, and lock indicators in one place.
  */
 static struct luna_state get_luna_state(const zmk_event_t *eh)
 {
     // If it’s a WPM event -> update wpm
     if (as_zmk_wpm_state_changed(eh)) {
         g_luna_state.wpm = zmk_wpm_get_state();
     }
 
     // If it’s a keycode event -> update mod bits
     if (as_zmk_keycode_state_changed(eh)) {
         // Use explicit_mods for older ZMK builds
         g_luna_state.mods = zmk_hid_get_explicit_mods();
     }
 
     // If it’s a HID indicators event -> update lock bits
     struct zmk_hid_indicators_changed *hid_ev = as_zmk_hid_indicators_changed(eh);
     if (hid_ev) {
         g_luna_state.indicators = hid_ev->indicators;
     }
 
     return g_luna_state;
 }
 
 /* Called whenever wpm, keycode, or hid indicators changed */
 static void luna_update_cb(struct luna_state s)
 {
     struct zmk_widget_luna *widget;
     SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
         set_animation(widget->obj, s);
     }
 }
 
 /* Register display listener */
 ZMK_DISPLAY_WIDGET_LISTENER(widget_luna, struct luna_state,
                             luna_update_cb, get_luna_state);
 
 /* Subscribe to wpm, keycode, and indicators */
 ZMK_SUBSCRIPTION(widget_luna, zmk_wpm_state_changed);
 ZMK_SUBSCRIPTION(widget_luna, zmk_keycode_state_changed);
 ZMK_SUBSCRIPTION(widget_luna, zmk_hid_indicators_changed);
 
 /* Standard widget init */
 int zmk_widget_luna_init(struct zmk_widget_luna *widget, lv_obj_t *parent)
 {
     widget->obj = lv_animimg_create(parent);
     // Example positioning. Adjust as you wish:
     lv_obj_align(widget->obj, LV_ALIGN_TOP_LEFT, 66, 22);
 
     sys_slist_append(&widgets, &widget->node);
     widget_luna_init();
     return 0;
 }
 
 lv_obj_t *zmk_widget_luna_obj(struct zmk_widget_luna *widget)
 {
     return widget->obj;
 }
 