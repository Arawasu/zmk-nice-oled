# NOTE
This fork includes several changes to mctechnology's zmk-nice-oled repo:
* Removed the WPM graph and typing activity meter.
* Retained battery level, Bluetooth connection status (with profile indicator), and current layer label.
* Luna is now horizontally centered and positioned lower on the screen to sit naturally above the other display elements.
* Added lv_font_montserrat_16 to enable larger, readable icons (like LV_SYMBOL_USB, LV_SYMBOL_WIFI). These are the same as in GPEye's hammerbeam repo.

# ORIGINAL README
![nice_oled_demo](./assets/nice_oled_demo.GIF)
![nice_epaper_demo](./assets/nice_epaper_demo.GIF)

## Table of Contents

<a name="top"></a>

- [Overview](#overview)
- [Gallery](#gallery)
- [Quick Installation](#quick-installation)
- [Configuration](#configuration)
- [Suggestions](#suggestions)
- [Limitations](#limitations)
- [Inspiration & Credits](#inspiration)
- [Support](#if-you-like-my-contributions-please-dont-forget-the-following)
- [My other projects](#my-other-projects)

# Overview
Vertical widgets for [oled] and [niceview] screens using zmk (for split and non-split keyboards)

Shields supported:

- `nice_oled`
- `nice_epaper`

General characteristics:

- **Vertical layout**
- **Compact**
- **HID widget**
- **Modifiers widget**
- **vertical animations for central and periphery**
- **Prevents your neck from hurting when looking at the keyboard**
- **Easy to read in the dark** (only `nice_oled`)

# Gallery

<div align="center">

  <img align="center" alt="demo3" width="400px" src="./assets/demo3.jpg" />
  <img align="center" alt="demo2" width="400px" src="./assets/demo2.jpg" />
  <img align="center" alt="demo1" width="400px" src="./assets/demo1.jpg" />
  <img align="center" alt="demo4" width="400px" src="./assets/demo4.jpg" />
  <img align="center" alt="Animation Preview" width="400px" src="./assets/animation.gif" />
  <img align="center" alt="preview1" width="400px" src="./assets/preview1.JPG" />

</div>

# Quick Installation
> [!TIP]
>
> All widgets are enabled `hid`,` modifiers`, `wpm` etc., if you want to
> disable them, look at the configuration section

Installation in 4 simple steps:

0. Configuration example? You should see this `->` [zmk-config]

1. Make sure to enable the custom status screen in your `*.conf` file:

```conf
CONFIG_ZMK_DISPLAY=y
CONFIG_ZMK_DISPLAY_STATUS_SCREEN_CUSTOM=y
```

2. Copy and paste this into your `config/west.yml` file:

```yaml
manifest:
  remotes:
    - name: zmkfirmware
      url-base: https://github.com/zmkfirmware
    # support nice_oled and nice_epaper
    - name: mctechnology17
      url-base: https://github.com/mctechnology17
  projects:
    - name: zmk
      remote: zmkfirmware
      revision: main
      import: app/west.yml
    # support nice_oled and nice_epaper
    - name: zmk-nice-oled
      remote: mctechnology17
      revision: main
  self:
    path: config
```

3. In the `build.yaml` file, add the `nice_oled` shield (using the corne as an
   example, but it could be another shield).

For [oled] screen:
```yaml
---
include:
  - board: nice_nano_v2
    shield: corne_left nice_oled
  - board: nice_nano_v2
    shield: corne_right nice_oled
```

For [niceview] screen:
```yaml
---
include:
  - board: nice_nano_v2
    shield: corne_left nice_view_adapter nice_epaper
  - board: nice_nano_v2
    shield: corne_right nice_view_adapter nice_epaper
```

4. Build the firmware, flash it to your keyboard, and enjoy!

# Configuration
> [!IMPORTANT]
> Make sure to enable the custom status screen in your ZMK configuration:

> [!TIP]
>
> 1. You can disable the gem animation if you find it annoying or want to save
>    battery life with the option `CONFIG_NICE_OLED_GEM_ANIMATION=n` in your `./config/corne.conf` file.
> 2. You can also deactivate it as follows `cmake-args:
>    -DCONFIG_NICE_OLED_GEM_ANIMATION=n` in your `build.yaml` file.
> 3. Remember that you can save battery by also turning off the screen and
>    activating deep sleep mode `CONFIG_ZMK_SLEEP=y
>    CONFIG_ZMK_IDLE_TIMEOUT=60000`. Here `60000` is the time in milliseconds,
>    in this case `1` minute `=` `(1*60*1000ms)`.

Make sure to enable the custom status screen in your ZMK configuration:

```conf
CONFIG_ZMK_DISPLAY=y
CONFIG_ZMK_DISPLAY_STATUS_SCREEN_CUSTOM=y
```

Modify the behavior of this shield by adjusting these options in your personal configuration files. For a more detailed explanation, refer to [Configuration in the ZMK documentation](https://zmk.dev/docs/config).

| Option                                                           | Type | Description                                                                                                                                                                                                                                                       | Default |
| ------------------------------------------                       | ---- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------- |
| `CONFIG_NICE_OLED_GRAPH_AND_NEEDLE_WPM_FIXED_RANGE`                 | bool | This shield uses a fixed range for the chart and gauge deflection. If you set this option to `n`, it will switch to a dynamic range, like the default nice!view shield, which dynamically adjusts based on the last 10 WPM values provided by ZMK.                | y       |
| `CONFIG_NICE_OLED_GRAPH_AND_NEEDLE_WPM_FIXED_RANGE_MAX`             | int  | You can adjust the maximum value of the fixed range to align with your current goal.                                                                                                                                                                              | 100     |
| `CONFIG_NICE_OLED_GEM_ANIMATION`                                 | bool | If you find the animation distracting (or want to save on battery usage), you can turn it off by setting this option to `n`. It will instead pick a random frame of the animation every time you restart your keyboard.                                           | y       |
| `CONFIG_NICE_OLED_GEM_ANIMATION_MS`                              | int  | Alternatively, you can slow down the animation. A high value, such as 96000, slows the animation considerably, showing the next frame every couple of seconds. The animation consists of 16 frames, and the default value of 960 milliseconds plays it at 60 fps. | 960     |
| `CONFIG_NICE_OLED_WIDGET_WPM`                                    | bool | Enables the Words Per Minute (WPM) widget on the OLED display.                                                                                                                                                                                                    | y       |
| `CONFIG_NICE_OLED_WIDGET_WPM_LUNA`                               | bool | Activates the Luna animation for the WPM widget.                                                                                                                                                                                                                  | y       |
| `CONFIG_NICE_OLED_WIDGET_WPM_LUNA_ANIMATION_MS`                  | int  | Sets the duration of the Luna animation for the WPM widget (in milliseconds).                                                                                                                                                                                     | 300     |
| `CONFIG_NICE_OLED_WIDGET_HID_INDICATORS`                         | bool | Enables the Human Interface Device (HID) indicators widget.                                                                                                                                                                                                       | y       |
| `CONFIG_NICE_OLED_WIDGET_HID_INDICATORS_LUNA`                    | bool | Activates the Luna animation for the HID indicators widget.                                                                                                                                                                                                       | y       |
| `CONFIG_NICE_OLED_WIDGET_HID_INDICATORS_LUNA_ONLY_CAPSLOCK`      | bool | Activates the Luna animation for the HID indicators widget [ONLY for CapsLock ](https://zmk.dev/docs/keymaps/list-of-keycodes#locks)                                                                                                                  | n       |
| `CONFIG_NICE_OLED_WIDGET_HID_INDICATORS_LUNA_ANIMATION_MS`       | int  | Sets the duration of the Luna animation for the HID indicators widget (in milliseconds).                                                                                                                                                                          | 300     |
| `CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS`                   | bool | Enables the modifiers indicators widget, which shows active modifier keys.                                                                                                                                                                                        | y       |
| `CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_LUNA`              | bool | Activates the Luna animation for the modifiers indicators widget.                                                                                                                                                                                                 | y       |
| `CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_LUNA_ANIMATION_MS` | int  | Sets the duration of the Luna animation for the modifiers indicators widget (in milliseconds).                                                                                                                                                                    | 300     |


You can deactivate luna the dog as follows (default is activated):
```conf
CONFIG_NICE_OLED_WIDGET_WPM=n
CONFIG_NICE_OLED_WIDGET_WPM_LUNA=n
CONFIG_NICE_OLED_WIDGET_HID_INDICATORS=n
CONFIG_NICE_OLED_WIDGET_HID_INDICATORS_LUNA=n
CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS=n
CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_LUNA=n
```

You can enable only the capslock if it is more appropriate for you:
```conf
CONFIG_NICE_OLED_WIDGET_HID_INDICATORS_LUNA_ONLY_CAPSLOCK=y
```

# Suggestions
If you have any implementation suggestion or something similar opens a
discussion

# Limitations
Due to the extensive use of animations and vertical conversions, the processor
is heavily taxed by the required computations. This has inspired me to adopt
techniques similar to those used in Super Nintendo games (as documented in the
NesDev Wiki) to achieve smoother performance.

# Inspiration
Inspired/forked by the work of @M165437 and @englmaxi . All credits for them
for the original design and implementation.

- [nice-view-gem](https://github.com/M165437/nice-view-gem) by @M165437
- [zmk-dongle-display](https://github.com/englmaxi/zmk-dongle-display.git) by @englmaxi
- [nice-view-battery](https://github.com/infely/nice-view-battery) by @infely

# If you like my contributions, please don't forget the following:

I appreciate if you follow me on [<img align="center" alt="GitHub" width="22px" src="https://github.com/mctechnology17/mctechnology17/blob/main/src/github.png" />][github] and [<img align="center" alt="YouTube" width="22px" src="https://github.com/mctechnology17/mctechnology17/blob/main/src/youtube.png" />][youtube] !

![Star](./assets/star.GIF)

# My other projects:
- [zmk-dongle-display-view]: Use your [niceview] vertical with very beautiful widgets
- [zmk-oled-adapter]: This module is an adapter for 128x32, 128x64 and 128x128
  OLED screens, to be used on keyboards with ZMK without having to modify the
  shields of the keyboards.
- [qmk-config] will be updated soon
- [qmk_userspace] will be updated soon
- [vimtools] swiss army knife for vim (features and settings that will make your life easier)
- [gm] cross-platform git manager with friendly user interface
- [vim-better-header] the best automated template
- [vim-executor] multilanguage code executor

If you enjoy my contributions, feel free to donate.
- [paypal]
- [sponsor]

<p align="right">
  <a href="#top">⬆️  back to Table of Contents</a>
</p>

<p align="center">
        <img src="./assets/Bottom.svg" alt="Bottom" />
</p>

[zmk-config]: https://github.com/mctechnology17/zmk-config
[zmk-dongle-display-view]: https://github.com/mctechnology17/zmk-dongle-display-view
[zmk-oled-adapter]: https://github.com/mctechnology17/zmk-oled-adapter

[niceview]: https://nicekeyboards.com/nice-view/
[oled]: https://keycapsss.com/keyboard-parts/parts/80/0.91-oled-lcd-display-128x32-ssd1306-i2c
[nice-view]: https://nicekeyboards.com/nice-view
[puchi_ble_v1]: (https://keycapsss.com/keyboard-parts/mcu-controller/202/puchi-ble-wireless-microcontroller-pro-micro-replacement?number=KC10157_SWITCH&c=18)
[seeeduino_xiao_ble]: (https://keycapsss.com/keyboard-parts/mcu-controller/212/seeed-studio-xiao-nrf52840-rp2040-esp32c3?number=KC10167_NRF)
[nice_nano_v2]: (https://nicekeyboards.com/nice-nano)
[keymap-editor]: https://nickcoutsos.github.io/keymap-editor/
[ZMK firmware]: https://github.com/zmkfirmware/zmk/
[ZMK documentation]: https://zmk.dev/docs/user-setup
[ZMK keycodes]: https://zmk.dev/docs/codes
[ZMK Discord]: https://zmk.dev/community/discord/invite

[qmk-config]: https://github.com/mctechnology17/qmk-config
[qmk_userspace]: https://github.com/mctechnology17/qmk_userspace

[git]: (https://github.com/git-guides/install-git)

[github]: https://github.com/mctechnology17
[twitter]: https://twitter.com/mctechnology17
[youtube]: https://www.youtube.com/c/mctechnology17
[instagram]: https://www.instagram.com/mctechnology17/
[facebook]: https://m.facebook.com/mctechnology17/
[reddit]: https://www.reddit.com/user/mctechnology17

[vim-executor]: https://github.com/mctechnology17/vim-executor
[vim-better-header]: https://github.com/mctechnology17/vim-better-header
[gm]: https://github.com/mctechnology17/gm
[vimtools]: https://github.com/mctechnology17/vimtools

[jailbreakrepo]: https://mctechnology17.github.io/
[uiglitch]: https://repo.packix.com/package/com.mctechnology.uiglitch/
[uiswitches]: https://repo.packix.com/package/com.mctechnology.uiswitches/
[uibadge]: https://repo.packix.com/package/com.mctechnology.uibadge/

[youtuberepo]: https://github.com/mctechnology17/youtube_repo_mc_technology

[sponsor]: https://github.com/sponsors/mctechnology17
[paypal]: https://www.paypal.me/mctechnology17
