cmake_minimum_required(VERSION 3.13.0)
project(simulator VERSION 0.1.0 LANGUAGES C)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(kernel)
add_subdirectory(app)

add_executable(simulator main.c)

target_link_libraries(simulator PRIVATE kernel)
target_link_libraries(simulator PRIVATE app)

# printf重定向到控制台，代价是双击exe时会弹出一个黑色的控制台窗口。终端中启动时打印信息会显示在终端上
target_link_options(simulator PRIVATE -mconsole)  
