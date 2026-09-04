add_library(kernel STATIC kernel.c)

add_subdirectory(osal)
add_subdirectory(ao)
add_subdirectory(third_library)