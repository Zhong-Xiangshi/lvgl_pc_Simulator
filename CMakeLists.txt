cmake_minimum_required(VERSION 3.13.0)
project(app VERSION 0.1.0 LANGUAGES C)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(third_library/lvgl)
add_subdirectory(third_library/freetype-2.14.1)
set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/third_library/SDL2-2.32.10")
find_package(SDL2 REQUIRED)

#LVGL链接SDL2
target_link_libraries(lvgl PUBLIC SDL2::SDL2main SDL2::SDL2-static) #顺序很重要

add_executable(app main.c ui/ui.c)
target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/ui)
target_link_libraries(app PRIVATE lvgl)
# printf重定向到控制台，代价是双击exe时会弹出一个黑色的控制台窗口。终端中启动时打印信息会显示在终端上
target_link_options(app PRIVATE -mconsole)  
