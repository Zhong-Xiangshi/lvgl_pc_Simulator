cmake_minimum_required(VERSION 3.13.0)
project(lvgl-pc VERSION 0.1.0 LANGUAGES C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(lvgl)

set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/SDL2-2.32.10")
find_package(SDL2 REQUIRED)
target_link_libraries(lvgl PUBLIC SDL2::SDL2main SDL2::SDL2-static) #顺序很重要

add_executable(lvgl-pc main.c ui/ui.c)

target_include_directories(lvgl-pc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/ui)

target_link_libraries(lvgl-pc PRIVATE lvgl)
