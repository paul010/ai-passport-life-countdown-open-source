idf_component_register(
    SRCS "main.c"
         "idle_power.c"
         "life_quest_app.c"
         "life_quest_model.c"
         "life_quest_ui.c"
         "life_quest_nav.c"
         "quest_sprites.c"
         "../assets/fonts/life_quest_font_11.c"
         "../assets/fonts/life_quest_font_14.c"
         "../assets/fonts/life_quest_font_18.c"
         "fap_console.c"
         "life_countdown_model.c"
         "profile_images.c"
         "ui_pixel.c"
         "ui_pixel_math.c"
    INCLUDE_DIRS "."
    REQUIRES bsp esp_timer nvs_flash esp_system esp_driver_usb_serial_jtag
)
