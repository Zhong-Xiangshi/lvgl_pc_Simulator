/*
 * ftmodule.h - 嵌入式裁剪版 FreeType 模块注册表(覆盖版)
 *
 * 通过 CMake 选项 FT_EMBED_STRIP 将该目录加入 freetype 的 include 搜索路径
 * 最前端,使 "#include <freetype/config/ftmodule.h>" 命中本文件而非 FreeType
 * 源码树中的默认 19 模块版本。内容源自 LVGL 上游为嵌入式/字体渲染准备的
 * third_library/lvgl/src/libs/freetype/ftmodule.h:
 *   1. truetype(tt) 驱动    - 渲染 .ttf/.ttc
 *   2. sfnt            - truetype 依赖的字体容器解析
 *   3. smooth 渲染器    - 抗锯齿渲染
 *
 * 未注册模块(type1/cff/cid/pcf/bdf/autofit/pshinter/sdf 等)在静态库链接时
 * 不会被提取进可执行文件,用于模拟嵌入式环境的 ROM 裁剪。
 * 注意:此配置下 .otf(type1/cff) 等非 truetype 字体无法渲染。
 */

FT_USE_MODULE(FT_Driver_ClassRec, tt_driver_class)
FT_USE_MODULE(FT_Module_Class, sfnt_module_class)
FT_USE_MODULE(FT_Renderer_Class, ft_smooth_renderer_class)

/* EOF */
