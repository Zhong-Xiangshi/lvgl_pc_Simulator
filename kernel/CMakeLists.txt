add_library(kernel STATIC kernel.c lvgl_service.c)
# 使 kernel.h(kernel 总初始化入口)对 kernel 消费方(main.c/app)可见
target_include_directories(kernel PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(osal)
add_subdirectory(ao)
add_subdirectory(pubsub)
add_subdirectory(third_library)

target_link_libraries(kernel PUBLIC lvgl)