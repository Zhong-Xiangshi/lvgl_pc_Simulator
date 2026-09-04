# 第三方库:uthash 家族(uthash/utarray/utstring/utlist/utstack/utringbuffer)
#
# 纯头文件宏库,无 .c 源文件可编:宏在调用处(应用 .c)实例化,
# 这里仅向 kernel 目标挂载头文件搜索路径。
# 注意:uthash/src 内分配器(malloc/free/realloc)已定制为 osal_*(走 OSAL,
# 见 osal.h 内存分配节),各 ut*.h 自带 #include "osal.h",调用方无需额外包含。
target_include_directories(kernel INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/uthash/src)
