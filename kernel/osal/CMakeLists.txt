# OSAL 操作系统抽象层(FreeRTOS 风格 API):kernel/osal 模块自管理
#
# 后端选择:当前实现为 pthread(winpthreads)。移植 FreeRTOS 时,
# 把 osal 库的源文件换成 src/osal_freertos.c 即可,对外 API 不变。

add_library(osal STATIC src/osal.c)
target_include_directories(osal PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
# pthread(winpthreads)依赖随 osal PUBLIC 传播给所有使用者
target_link_libraries(osal PUBLIC pthread)

# 后端验证程序:独立运行,不依赖 LVGL/SDL。
# 换后端后回归验证:
#   cmake --build . --target osal_test && ./osal_test.exe   (退出码 0 = 全 PASS)
add_executable(osal_test test/osal_test.c)
target_link_libraries(osal_test PRIVATE osal)
