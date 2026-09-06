

add_library(app STATIC)
target_sources(app PRIVATE app.c ui/ui.c)
target_include_directories(app PUBLIC .)
target_link_libraries(app PRIVATE kernel)
