add_library(freetype 
    src/base/ftbase.c
    src/base/ftbitmap.c
    src/base/ftdebug.c
    src/base/ftglyph.c
    src/base/ftinit.c
    src/base/ftstroke.c
    src/cache/ftcache.c
    src/gzip/ftgzip.c
    src/sfnt/sfnt.c
    src/smooth/smooth.c
    src/truetype/truetype.c
    ${CMAKE_SOURCE_DIR}/lvgl-9.3.0/src/libs/freetype/lv_ftsystem.c
    src/base/ftmm.c
)


# 添加头文件路径 (-I)
target_include_directories(freetype PUBLIC include ${CMAKE_SOURCE_DIR})
# 添加宏定义 (-D)
target_compile_definitions(freetype PUBLIC
    FT2_BUILD_LIBRARY
    # 注意：包含路径和尖括号的定义需要加上双引号
    "FT_CONFIG_MODULES_H=<lvgl-9.3.0/src/libs/freetype/ftmodule.h>"
    "FT_CONFIG_OPTIONS_H=<lvgl-9.3.0/src/libs/freetype/ftoption.h>"
)