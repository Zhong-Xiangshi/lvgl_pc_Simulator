add_library(framework STATIC framework.c lvgl_service.c)
target_include_directories(framework PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(third_library)

target_link_libraries(framework PUBLIC kernel)
target_link_libraries(framework PUBLIC lvgl)