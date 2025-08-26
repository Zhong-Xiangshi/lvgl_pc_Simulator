cmake_minimum_required(VERSION 3.10.0)
project(lvgl-pc VERSION 0.1.0 LANGUAGES C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 递归查找目录下所有的 .c 文件
file(GLOB_RECURSE UI_FILES "ui/*.c")

add_executable(lvgl-pc 
    main.c 
    ${UI_FILES}
)

target_include_directories(lvgl-pc PRIVATE
    .
    lib
    ui
)

target_link_libraries(lvgl-pc PRIVATE
    lvgl 
    lvgl_demos 
    lvgl_examples
    lv_drivers
)

# printf重定向到控制台，代价是双击exe时会弹出一个黑色的控制台窗口。终端中启动时打印信息会显示在终端上
target_link_options(lvgl-pc PRIVATE -mconsole)

add_subdirectory(lib/lvgl)
add_subdirectory(lib/lv_drivers)