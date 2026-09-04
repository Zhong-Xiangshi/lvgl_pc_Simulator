# OSAL 操作系统抽象层(FreeRTOS 风格 API):kernel/osal 模块自管理
#
# 后端选择:当前实现为 pthread(winpthreads)。移植 FreeRTOS 时,

target_sources(kernel PRIVATE src/osal.c)
target_include_directories(kernel PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(kernel PRIVATE pthread)

# 后端验证程序:独立运行,不依赖 LVGL/SDL。
# 换后端后回归验证:
#   cmake --build . --target osal_test && ./osal_test.exe   (退出码 0 = 全 PASS)
# add_executable(osal_test test/osal_test.c)
# target_link_libraries(osal_test PRIVATE kernel)
