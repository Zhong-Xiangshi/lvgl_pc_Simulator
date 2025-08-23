cmake_minimum_required(VERSION 3.10.0)
project(lvgl-pc VERSION 0.1.0 LANGUAGES C)

add_executable(lvgl-pc main.c)
target_link_libraries(lvgl-pc PRIVATE lvgl)

target_include_directories(lvgl-pc PRIVATE ui)
target_sources(lvgl-pc PRIVATE ui/ui.c)

# printf重定向到控制台，代价是双击exe时会弹出一个黑色的控制台窗口。终端中启动时打印信息会显示在终端上
target_link_options(lvgl-pc PRIVATE -mconsole)  

add_subdirectory(lvgl-9.3.0)