LVGL PC模拟器，已链接好SDL和FREETYPE和LVGL库。
## 使用方法：
1. [Releases · niXman/mingw-builds-binaries](https://github.com/niXman/mingw-builds-binaries/releases) 下载编译器，并添加bin目录到环境变量
2. vscode安装cmake tools拓展，打开项目
3. ctrl+shift+p，“cmake 选择工具包”，选择编译器
4. 点击左下角生成和运行

## 文件系统&&图片&&字体
可以使用LVGL**内置的**第三方解码库（直接打开宏定义就可以使用）
在lv_conf.h中修改：
1. win文件系统驱动 LV_USE_FS_WIN32
``` c
#define LV_USE_FS_WIN32 1
#if LV_USE_FS_WIN32
    #define LV_FS_WIN32_LETTER 'A'     /**< Set an upper-case driver-identifier letter for this driver (e.g. 'A'). */
    #define LV_FS_WIN32_PATH "D:/work/lvgl_pc_Simulator/resource"         /**< Set the working directory. File/directory paths will be appended to it. */
    #define LV_FS_WIN32_CACHE_SIZE 0    /**< >0 to cache this number of bytes in lv_fs_read() */
#endif
```
2. 字体  
- [LV_USE_TINY_TTF](https://lvgl.100ask.net/master/details/libs/tiny_ttf.html)  
``` c
#define LV_USE_TINY_TTF 1
#if LV_USE_TINY_TTF
    /* Enable loading TTF data from files */
    #define LV_TINY_TTF_FILE_SUPPORT 1
    #define LV_TINY_TTF_CACHE_GLYPH_CNT 256
#endif
```
- LV_USE_FREETYPE 已链接好，直接改1即可。比较TINY_TTF快8倍，建议优先使用


3. 图片 [LV_USE_LODEPNG](https://lvgl.100ask.net/master/details/libs/lodepng.html)
``` c
/** LODEPNG decoder library */
#define LV_USE_LODEPNG 1
```
### 例子
``` c
//img
lv_obj_t* img = lv_img_create(lv_scr_act());
lv_obj_center(img);
lv_img_set_src(img, "A:/images/waring.png");

//tinyttf
lv_font_t *ttf=lv_tiny_ttf_create_file("A:/fonts/NotoSansSC-VariableFont_wght.ttf", 40);
lv_obj_t* label = lv_label_create(lv_scr_act());
lv_label_set_text(label, "你好 LVGL!");
lv_obj_set_style_text_font(label, ttf, 0);
lv_obj_align(label, LV_ALIGN_CENTER, 0, -50);

//freetype
//拷贝文件到内存函数
lv_fs_res_t lvfs_load_file_to_mem(const char *path, uint8_t **out_buf, uint32_t *out_size)
{
    if(!path || !out_buf || !out_size) return LV_FS_RES_INV_PARAM;

    *out_buf  = NULL;
    *out_size = 0;

    lv_fs_file_t f;
    lv_fs_res_t res = lv_fs_open(&f, path, LV_FS_MODE_RD);
    if(res != LV_FS_RES_OK) return res;

    // 计算文件大小
    uint32_t old_pos = 0;
    (void)lv_fs_tell(&f, &old_pos);                // 记录当前位置（仅防御性）
    res = lv_fs_seek(&f, 0, LV_FS_SEEK_END);
    if(res != LV_FS_RES_OK) { lv_fs_close(&f); return res; }

    uint32_t size = 0;
    res = lv_fs_tell(&f, &size);
    if(res != LV_FS_RES_OK) { lv_fs_close(&f); return res; }

    // 空文件直接返回
    if(size == 0) {
        (void)lv_fs_seek(&f, old_pos, LV_FS_SEEK_SET);
        lv_fs_close(&f);
        *out_buf = NULL;
        *out_size = 0;
        return LV_FS_RES_OK;
    }

    // 分配内存
    uint8_t *buf = (uint8_t *)lv_mem_alloc(size);
    if(!buf) {
        (void)lv_fs_seek(&f, old_pos, LV_FS_SEEK_SET);
        lv_fs_close(&f);
        LV_LOG_ERROR("LV_FS_RES_OUT_OF_MEM");
        return LV_FS_RES_OUT_OF_MEM;
    }

    // 回到文件开头并读取（分块，兼容某些驱动的单次读取限制）
    res = lv_fs_seek(&f, 0, LV_FS_SEEK_SET);
    if(res != LV_FS_RES_OK) { lv_mem_free(buf); lv_fs_close(&f); return res; }

    uint32_t total = 0;
    const uint32_t CHUNK = 4096; // 可按需调整
    while(total < size) {
        uint32_t to_read = size - total;
        if(to_read > CHUNK) to_read = CHUNK;

        uint32_t br = 0; // bytes read
        res = lv_fs_read(&f, buf + total, to_read, &br);
        if(res != LV_FS_RES_OK) { lv_mem_free(buf); lv_fs_close(&f); return res; }

        if(br == 0) break; // 意外 EOF
        total += br;
    }

    lv_fs_close(&f);

    if(total != size) {
        lv_mem_free(buf);
        return LV_FS_RES_FS_ERR; // 读取不完整
    }

    *out_buf  = buf;
    *out_size = size;
    return LV_FS_RES_OK;
}

uint8_t *font_cache = NULL;
uint32_t font_size = 0;
lv_fs_res_t ret=lvfs_load_file_to_mem("s:/resources/fonts/Roboto_Condensed-Regular.ttf", &font_cache, &font_size);

static lv_ft_info_t info;
info.name = "fonts.ttf";
info.weight = 60;
info.style = FT_FONT_STYLE_NORMAL;
info.mem = font_cache;
info.mem_size = font_size;
if(!lv_ft_font_init(&info)) {
    LV_LOG_ERROR("create failed.");
}

lv_obj_t* label = lv_label_create(lv_scr_act());
lv_label_set_text(label, "hello LVGL!");
lv_obj_set_style_text_font(label, info.font, 0);
lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
```