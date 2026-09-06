# 第三方库:uthash 家族(uthash/utarray/utstring/utlist/utstack/utringbuffer)
#
# 纯头文件宏库,无 .c 源文件可编:宏在调用处(应用 .c)实例化,
# 这里仅向 kernel 目标挂载头文件搜索路径。
# 注意:uthash/src 内分配器(malloc/free/realloc)已定制为 osal_*(走 OSAL,
# 见 osal.h 内存分配节),各 ut*.h 自带 #include "osal.h",调用方无需额外包含。
# PUBLIC:kernel 自身源文件(pubsub.c 为首个 uthash 消费者)与消费方均可包含
target_include_directories(kernel PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/uthash/src)

# 模拟嵌入式裁剪开关:ON=默认,禁用 FreeType 全部外部依赖(zlib/png/bzip2/harfbuzz/brotli),
# 仅注册 truetype/sfnt/smooth 模块(本工程只渲染 .ttf,裁剪后 simulator.exe 体积约小 28%);
# OFF=桌面全量,自动探测外部依赖并注册全部 19 个模块(可渲染 .otf/.cff 等,
# 需要系统装有对应依赖库)。两种模式均已验证。切换后需重新 configure。
option(FT_EMBED_STRIP "模拟嵌入式场景:裁剪 FreeType 外部依赖与模块" ON)
if(FT_EMBED_STRIP)
    # 在 add_subdirectory(freetype) 前预置,禁用其内部 find_package(依赖探测)
    set(FT_DISABLE_ZLIB TRUE CACHE BOOL "" FORCE)
    set(FT_DISABLE_PNG TRUE CACHE BOOL "" FORCE)
    set(FT_DISABLE_BZIP2 TRUE CACHE BOOL "" FORCE)
    set(FT_DISABLE_HARFBUZZ TRUE CACHE BOOL "" FORCE)
    set(FT_DISABLE_BROTLI TRUE CACHE BOOL "" FORCE)
endif()

# 必须用 CACHE + FORCE 预置：lvgl 的 os_desktop.cmake 会 set(LV_BUILD_CONF_PATH "" CACHE PATH ...)，
# 若缓存中无此项,该 CACHE set 会在 lvgl 作用域内创建同名普通变量(空值)遮蔽父级的值;
# --fresh 清空缓存后必现。FORCE 预置后 lvgl 的 set 因缓存已存在而不生效。
set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h" CACHE PATH "lv_conf.h 路径" FORCE)

add_subdirectory(lvgl)
add_subdirectory(freetype-2.14.3)
if(FT_EMBED_STRIP)
    # 用覆盖版 ftmodule.h 裁模块:该目录置于 freetype include 搜索最前端,
    # 使 #include <freetype/config/ftmodule.h> 命中 freetype_embed/ 下的精简注册表,
    # 未注册模块在静态链接时不会被提取。用 include 覆盖而非 -D 宏注入,
    # 避免宏进入 .rc 资源的编译(windres)导致失败。
    target_include_directories(freetype BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/freetype_embed)
endif()
set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/SDL2-2.32.10")
find_package(SDL2 REQUIRED)

# LVGL链接SDL2。注意:只链 SDL2-static,不要链 SDL2::SDL2main ——
# SDL2main 会对 exe 注入 -Wl,--undefined=WinMain 并期望应用入口叫 SDL_main,
# 而本工程入口是 main.c 的普通 main(控制台子系统,-mconsole),lvgl 的 SDL 驱动
# (lv_sdl_window.c)也自带 #define SDL_MAIN_HANDLED 走"自管入口"路线,二者矛盾,
# 链接时会报 undefined reference to `SDL_main'
target_link_libraries(lvgl PUBLIC SDL2::SDL2-static)
#LVGL链接FreeType
target_link_libraries(lvgl PRIVATE freetype)
# lvgl 的 FT 移植层(lv_ftsystem.c)使用了 FreeType 内部头文件/macro,
# 编译时必须处于"构建 FreeType 库本身"的状态(否则 fterrors.h 会清除 FT_ERR_PREFIX)
target_compile_definitions(lvgl PRIVATE FT2_BUILD_LIBRARY)
