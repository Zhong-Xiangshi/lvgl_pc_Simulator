# 主动对象库:单头文件,基于 osal 的线程委托封装
add_library(ao INTERFACE)
target_include_directories(ao INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(ao INTERFACE osal)   # osal 的 include 与 pthread 依赖透传给使用者

# 验证程序:独立运行,不依赖 LVGL/SDL。
#   cmake --build . --target ao_test && ./ao_test.exe   (退出码 0 = 全 PASS)
add_executable(ao_test test/ao_test.c)
target_link_libraries(ao_test PRIVATE ao)
