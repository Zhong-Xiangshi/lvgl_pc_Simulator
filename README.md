LVGL PC模拟器，已链接好SDL和LVGL库。
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
    #define LV_FS_WIN32_PATH "D:/work/lvgl_pc_Simulator/resource/"         /**< Set the working directory. File/directory paths will be appended to it. */
    #define LV_FS_WIN32_CACHE_SIZE 0    /**< >0 to cache this number of bytes in lv_fs_read() */
#endif
```
2. 字体 LV_USE_TINY_TTF https://lvgl.100ask.net/master/details/libs/tiny_ttf.html
``` c
#define LV_USE_TINY_TTF 1
#if LV_USE_TINY_TTF
    /* Enable loading TTF data from files */
    #define LV_TINY_TTF_FILE_SUPPORT 1
    #define LV_TINY_TTF_CACHE_GLYPH_CNT 256
#endif
```
3. 图片 LV_USE_LODEPNG https://lvgl.100ask.net/master/details/libs/lodepng.html
``` c
/** LODEPNG decoder library */
#define LV_USE_LODEPNG 1
```
### 例子
``` c
lv_obj_t* img = lv_image_create(lv_scr_act());
lv_obj_center(img);
lv_image_set_src(img, "A:images/waring.png");

lv_font_t *ttf=lv_tiny_ttf_create_file("A:fonts/NotoSansSC-VariableFont_wght.ttf", 40);
lv_obj_t* label = lv_label_create(lv_scr_act());
lv_label_set_text(label, "你好 LVGL!");
lv_obj_set_style_text_font(label, ttf, 0);
lv_obj_align(label, LV_ALIGN_CENTER, 0, -50);
```