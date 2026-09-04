# pubsub 发布-订阅消息总线:单例 broker AO + 两级 uthash,依赖 osal/ao/uthash
target_sources(kernel PRIVATE src/pubsub.c)
target_include_directories(kernel PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
