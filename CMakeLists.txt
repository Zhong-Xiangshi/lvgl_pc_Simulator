cmake_minimum_required(VERSION 3.13.0)
project(app VERSION 0.1.0 LANGUAGES C)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(third_library/lvgl)
add_subdirectory(third_library/freetype-2.14.3)
set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/third_library/SDL2-2.32.10")
find_package(SDL2 REQUIRED)

#LVGL链接SDL2
target_link_libraries(lvgl PUBLIC SDL2::SDL2main SDL2::SDL2-static) #顺序很重要
#LVGL链接FreeType
target_link_libraries(lvgl PUBLIC freetype)
# lvgl 的 FT 移植层(lv_ftsystem.c)使用了 FreeType 内部头文件/macro,
# 编译时必须处于"构建 FreeType 库本身"的状态(否则 fterrors.h 会清除 FT_ERR_PREFIX)
target_compile_definitions(lvgl PRIVATE FT2_BUILD_LIBRARY)

add_executable(app main.c ui/ui.c)
target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/ui)
target_link_libraries(app PRIVATE lvgl)
# 强制链接器使用 lvgl 内的 lv_ftsystem.o 提供 FreeType 系统层(内存走 lv_malloc、文件流走 lv_fs)。
# 这3个符号先由 liblvgl.a 解析（依赖 liblvgl.a 排在 libfreetype.a 前面）,链接器便不会再从 libfreetype.a 提取官方 ftsystem.o,实现 LV_FREETYPE_USE_LVGL_PORT 的替换
target_link_options(app PRIVATE
    -Wl,--undefined=FT_New_Memory
    -Wl,--undefined=FT_Done_Memory
    -Wl,--undefined=FT_Stream_Open)
# printf重定向到控制台，代价是双击exe时会弹出一个黑色的控制台窗口。终端中启动时打印信息会显示在终端上
target_link_options(app PRIVATE -mconsole)  
