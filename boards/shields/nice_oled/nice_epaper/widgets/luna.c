/*
 * Single-luna with:
 *  - WPM-based idle/walk/run
 *  - Direct real-time modifier tracking (Shift/Ctrl/Alt/GUI) by hooking key events
 *  - HID lock-based bark (CapsLock/NumLock/ScrollLock)
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
 
 // HID usage IDs for left/right modifiers (standard USB HID)
 #define HID_USAGE_KEY_LEFT_CONTROL   0xE0
 #define HID_USAGE_KEY_LEFT_SHIFT     0xE1
 #define HID_USAGE_KEY_LEFT_ALT       0xE2
 #define HID_USAGE_KEY_LEFT_GUI       0xE3
 
 #define HID_USAGE_KEY_RIGHT_CONTROL  0xE4
 #define HID_USAGE_KEY_RIGHT_SHIFT    0xE5
 #define HID_USAGE_KEY_RIGHT_ALT      0xE6
 #define HID_USAGE_KEY_RIGHT_GUI      0xE7
 
 // Bits to represent each mod (ZMK style)
 #define MY_MOD_LCTRL (1 << 0)
 #define MY_MOD_RCTRL (1 << 1)
 #define MY_MOD_LSHIFT (1 << 2)
 #define MY_MOD_RSHIFT (1 << 3)
 #define MY_MOD_LALT (1 << 4)
 #define MY_MOD_RALT (1 << 5)
 #define MY_MOD_LGUI (1 << 6)
 #define MY_MOD_RGUI (1 << 7)
 
 // Map from usage to our local bits
 static inline uint8_t usage_to_mod_bit(uint8_t usage)
 {
     switch (usage) {
     case HID_USAGE_KEY_LEFT_CONTROL:  return MY_MOD_LCTRL;
     case HID_USAGE_KEY_RIGHT_CONTROL: return MY_MOD_RCTRL;
     case HID_USAGE_KEY_LEFT_SHIFT:    return MY_MOD_LSHIFT;
     case HID_USAGE_KEY_RIGHT_SHIFT:   return MY_MOD_RSHIFT;
     case HID_USAGE_KEY_LEFT_ALT:      return MY_MOD_LALT;
     case HID_USAGE_KEY_RIGHT_ALT:     return MY_MOD_RALT;
     case HID_USAGE_KEY_LEFT_GUI:      return MY_MOD_LGUI;
     case HID_USAGE_KEY_RIGHT_GUI:     return MY_MOD_RGUI;
     default: return 0;
     }
 }
 
 // Convert our local bits to "ZMK mod constants" for SHIFT, CTRL, etc.
 // We want SHIFT => (MOD_LSFT | MOD_RSFT), CTRL => (MOD_LCTL | MOD_RCTL), etc.
 static uint8_t build_zmk_mod_bits(uint8_t local_bits)
 {
     uint8_t zmk_mods = 0;
     // SHIFT
     if (local_bits & (MY_MOD_LSHIFT | MY_MOD_RSHIFT)) {
         zmk_mods |= (MOD_LSFT | MOD_RSFT);
     }
     // CTRL
     if (local_bits & (MY_MOD_LCTRL | MY_MOD_RCTRL)) {
         zmk_mods |= (MOD_LCTL | MOD_RCTL);
     }
     // ALT
     if (local_bits & (MY_MOD_LALT | MY_MOD_RALT)) {
         zmk_mods |= (MOD_LALT | MOD_RALT);
     }
     // GUI
     if (local_bits & (MY_MOD_LGUI | MY_MOD_RGUI)) {
         zmk_mods |= (MOD_LGUI | MOD_RGUI);
     }
     return zmk_mods;
 }
 
 /* Now we define the dog frames... */
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
 static const lv_img_dsc_t *mid_imgs[]  =  { &dog_walk1_90, &dog_walk2_90 }; // could differ
 static const lv_img_dsc_t *fast_imgs[] =  { &dog_run1_90,  &dog_run2_90  };
 
 // Modifier override frames
 static const lv_img_dsc_t *mod_sit[]   =  { &dog_sit1_90,   &dog_sit2_90   };
 static const lv_img_dsc_t *mod_walk[]  =  { &dog_walk1_90,  &dog_walk2_90  };
 static const lv_img_dsc_t *mod_run[]   =  { &dog_run1_90,   &dog_run2_90   };
 static const lv_img_dsc_t *mod_sneak[] =  { &dog_sneak1_90, &dog_sneak2_90 };
 
 // HID locks → bark
 static const lv_img_dsc_t *bark_imgs[] = { &dog_bark1_90, &dog_bark2_90 };
 
 #define MY_ARRSZ(arr) (sizeof(arr) / sizeof(arr[0]))
 
 enum anim_state {
     ANIM_NONE,
     ANIM_IDLE,
     ANIM_SLOW,
     ANIM_MID,
     ANIM_FAST,
     ANIM_OVERRIDE
 };
 
 static enum anim_state current_anim_state = ANIM_NONE;
 static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);
 
 // This is the unified state
 struct luna_state {
     uint8_t wpm;
 
     // Our local store of pressed mod bits, e.g. MY_MOD_LSHIFT, MY_MOD_RALT, etc.
     // We'll convert these to zmk's "MOD_LSFT" etc. for the override logic
     uint8_t local_mod_bits;
 
     // CapsLock / NumLock / ScrollLock bits
     uint8_t indicators;
 };
 static struct luna_state g_luna_state = {0, 0, 0};
 
 // SHIFT → SNEAK, CTRL → RUN, ALT → WALK, GUI → SIT
 static const lv_img_dsc_t **get_modifier_frames(uint8_t zmk_mods, size_t *count_out)
 {
     if (zmk_mods & (MOD_LSFT | MOD_RSFT)) {
         *count_out = MY_ARRSZ(mod_sneak);
         return mod_sneak;
     } else if (zmk_mods & (MOD_LCTL | MOD_RCTL)) {
         *count_out = MY_ARRSZ(mod_run);
         return mod_run;
     } else if (zmk_mods & (MOD_LALT | MOD_RALT)) {
         *count_out = MY_ARRSZ(mod_walk);
         return mod_walk;
     } else if (zmk_mods & (MOD_LGUI | MOD_RGUI)) {
         *count_out = MY_ARRSZ(mod_sit);
         return mod_sit;
     }
     return NULL;
 }
 
 /* Decide which frames to animate, in order:
  * 1) HID locks => bark
  * 2) Mod overrides => one of sit/walk/run/sneak
  * 3) WPM => idle/walk/run
  */
 static void set_animation(lv_obj_t *animimg, struct luna_state s)
 {
     // Step 1: Caps/Num/Scroll => bark
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
 
     // Step 2: If any real-time mods => override
     uint8_t zmk_mods = build_zmk_mod_bits(s.local_mod_bits);
     size_t frames_count = 0;
     const lv_img_dsc_t **frames = get_modifier_frames(zmk_mods, &frames_count);
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
 
     // Step 3: fallback to WPM
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
  * This function is the aggregator for all relevant events:
  * - zmk_wpm_state_changed => update WPM
  * - zmk_keycode_state_changed => track press or release of mods
  * - zmk_hid_indicators_changed => track caps/num/scroll
  */
 static struct luna_state get_luna_state(const zmk_event_t *eh)
 {
     // 1) If it's a WPM event => update wpm
     if (as_zmk_wpm_state_changed(eh)) {
         g_luna_state.wpm = zmk_wpm_get_state();
     }
 
     // 2) If it's a keycode event => track mod usage ourselves
     struct zmk_keycode_state_changed *kc_ev = as_zmk_keycode_state_changed(eh);
     if (kc_ev) {
         // usage ID is just ev->keycode if usage_page == 0x07, typically
         // But let's confirm usage_page? If it's not 0x07, it's not a standard mod.
         if (kc_ev->usage_page == HID_USAGE_KEY) {
             uint8_t bit = usage_to_mod_bit(kc_ev->keycode);
             if (bit) {
                 if (kc_ev->state) {
                     // Pressed => set that bit
                     g_luna_state.local_mod_bits |= bit;
                 } else {
                     // Released => clear that bit
                     g_luna_state.local_mod_bits &= ~bit;
                 }
             }
         }
     }
 
     // 3) If it's a HID indicators event => update lock bits
     struct zmk_hid_indicators_changed *hid_ev = as_zmk_hid_indicators_changed(eh);
     if (hid_ev) {
         g_luna_state.indicators = hid_ev->indicators;
     }
 
     return g_luna_state;
 }
 
 /*
  * Whenever any relevant event fires, we run set_animation() with the updated state
  */
 static void luna_update_cb(struct luna_state s)
 {
     struct zmk_widget_luna *widget;
     SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
         set_animation(widget->obj, s);
     }
 }
 
 /* Register with the display pipeline */
 ZMK_DISPLAY_WIDGET_LISTENER(widget_luna, struct luna_state,
                             luna_update_cb, get_luna_state);
 
 /* Subscribe to WPM, keycode, and HID lock changes */
 ZMK_SUBSCRIPTION(widget_luna, zmk_wpm_state_changed);
 ZMK_SUBSCRIPTION(widget_luna, zmk_keycode_state_changed);
 ZMK_SUBSCRIPTION(widget_luna, zmk_hid_indicators_changed);
 
 /* Standard widget init */
 int zmk_widget_luna_init(struct zmk_widget_luna *widget, lv_obj_t *parent)
 {
     widget->obj = lv_animimg_create(parent);
     // Tweak position as desired
     lv_obj_align(widget->obj, LV_ALIGN_TOP_LEFT, 66, 22);
 
     sys_slist_append(&widgets, &widget->node);
     widget_luna_init();
     return 0;
 }
 
 lv_obj_t *zmk_widget_luna_obj(struct zmk_widget_luna *widget)
 {
     return widget->obj;
 }
 