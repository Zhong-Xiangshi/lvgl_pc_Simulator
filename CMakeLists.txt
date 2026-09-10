cmake_minimum_required(VERSION 3.13.0)
project(simulator VERSION 0.1.0 LANGUAGES C)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(kernel)
add_subdirectory(framework)
add_subdirectory(app)

add_executable(simulator main.c)

target_link_libraries(simulator PRIVATE kernel)
target_link_libraries(simulator PRIVATE framework)
target_link_libraries(simulator PRIVATE app)

# 强制链接器提取 lvgl 内的 lv_ftsystem.o 提供 FreeType 系统层(内存走 lv_malloc、文件流走 lv_fs)。
# 这3个符号一旦先由 liblvgl.a 解析（liblvgl.a 必须排在 libfreetype.a 前面），链接器便不会再从
# libfreetype.a 提取官方 ftsystem.o，实现 LV_FREETYPE_USE_LVGL_PORT 的替换。
# 注意：必须加在最终可执行目标上(PRIVATE 对静态库不生效，之前放 kernel 静态库上导致标志丢失)
target_link_options(simulator PRIVATE
    -Wl,--undefined=FT_New_Memory
    -Wl,--undefined=FT_Done_Memory
    -Wl,--undefined=FT_Stream_Open)

# printf重定向到控制台，代价是双击exe时会弹出一个黑色的控制台窗口。终端中启动时打印信息会显示在终端上
target_link_options(simulator PRIVATE -mconsole)
