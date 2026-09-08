# 主动对象库:单头文件,基于 osal 的线程委托封装
# PUBLIC:kernel 自身源文件(pubsub.c)与消费方均可 #include "ao.h"
target_include_directories(kernel PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
