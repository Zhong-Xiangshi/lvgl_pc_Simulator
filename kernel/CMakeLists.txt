add_library(kernel STATIC kernel.c lvgl_service.c)
# 使 kernel.h(kernel 总初始化入口)对 kernel 消费方(main.c/app)可见
target_include_directories(kernel PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(osal)
add_subdirectory(ao)
add_subdirectory(pubsub)
add_subdirectory(third_library)

target_link_libraries(kernel PUBLIC lvgl)

# 强制链接器使用 lvgl 内的 lv_ftsystem.o 提供 FreeType 系统层(内存走 lv_malloc、文件流走 lv_fs)。
# 这3个符号先由 liblvgl.a 解析（依赖 liblvgl.a 排在 libfreetype.a 前面）,链接器便不会再从 libfreetype.a 提取官方 ftsystem.o,实现 LV_FREETYPE_USE_LVGL_PORT 的替换
target_link_options(kernel PRIVATE
    -Wl,--undefined=FT_New_Memory
    -Wl,--undefined=FT_Done_Memory
    -Wl,--undefined=FT_Stream_Open)