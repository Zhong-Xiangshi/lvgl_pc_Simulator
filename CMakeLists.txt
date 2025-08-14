cmake_minimum_required(VERSION 3.10.0)
project(lvgl-pc VERSION 0.1.0 LANGUAGES C)

add_executable(lvgl-pc main.c)
target_link_libraries(lvgl-pc PRIVATE lvgl)

target_include_directories(lvgl-pc PRIVATE ui)
target_sources(lvgl-pc PRIVATE ui/ui.c)

set(LV_BUILD_CONF_PATH lvgl-9.3.0/lv_conf.h)
add_subdirectory(lvgl-9.3.0)