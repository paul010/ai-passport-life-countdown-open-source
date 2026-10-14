cmake_minimum_required(VERSION 3.16)
project(life_quest_render C)
set(CMAKE_C_STANDARD 11)
set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_LIST_DIR}/lv_conf.h" CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
set(ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
add_subdirectory("${ROOT}/managed_components/lvgl__lvgl" lvgl)
add_executable(life_quest_render render.c
  "${ROOT}/main/life_quest_ui.c" "${ROOT}/main/life_quest_nav.c"
  "${ROOT}/main/life_quest_model.c" "${ROOT}/main/life_countdown_model.c"
  "${ROOT}/main/profile_images.c" "${ROOT}/main/quest_sprites.c"
  "${ROOT}/main/ui_pixel.c" "${ROOT}/main/ui_pixel_math.c"
  "${ROOT}/assets/fonts/life_quest_font_11.c"
  "${ROOT}/assets/fonts/life_quest_font_14.c"
  "${ROOT}/assets/fonts/life_quest_font_18.c")
target_include_directories(life_quest_render PRIVATE "${ROOT}/main")
target_compile_options(life_quest_render PRIVATE -UNDEBUG)
target_link_libraries(life_quest_render PRIVATE lvgl m)
