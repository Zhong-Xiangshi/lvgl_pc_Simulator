target_sources(lvgl PRIVATE
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
    src/base/ftmm.c
)

# 把 FreeType 的头文件路径也加给 lvgl
target_include_directories(lvgl PUBLIC include)

# 把宏定义也直接加给 lvgl
target_compile_definitions(lvgl PUBLIC
    FT2_BUILD_LIBRARY
    "FT_CONFIG_MODULES_H=<src/libs/freetype/ftmodule.h>"
    "FT_CONFIG_OPTIONS_H=<src/libs/freetype/ftoption.h>"
)