if(CONFIG_ZMK_DISPLAY AND CONFIG_NICE_VIEW_WIDGET_STATUS)
  zephyr_library_include_directories(${CMAKE_SOURCE_DIR}/include)

  zephyr_library_sources(custom_status_screen.c)
  zephyr_library_sources(assets/images.c)
  zephyr_library_sources(widgets/util.c)

  zephyr_library_sources(widgets/layer.c)
  zephyr_library_sources(widgets/profile.c)
  zephyr_library_sources(widgets/screen.c)
  zephyr_library_sources(widgets/wpm.c)
  zephyr_library_sources(assets/luna_images.c)
  target_sources_ifdef(CONFIG_NICE_OLED_WIDGET_WPM app PRIVATE widgets/luna.c)

endif()
