# EasyLogger 日志库 2.2.0(裁剪配置见 inc/elog_cfg.h):
#   1) 异步输出线程由 src/elog_async.c 用 osal_task/osal_sem 适配实现(顶部有说明),
#      官方 pthread 版不启用;
#   2) 缓冲模式未启用,不编译 src/elog_buf.c;
#   3) port/elog_port.c 仅依赖 osal_printf/osal_mutex,FreeRTOS 移植换 osal 后端
#      并按需改时间/线程信息实现,库零改动。
# 注意:依赖 kernel 目标上的 PUBLIC include 路径(osal.h),故必须由 kernel 目标编译;
# 本文件经 kernel/third_library/CMakeLists.txt 的 add_subdirectory 挂入。
target_sources(kernel PRIVATE
    src/elog.c
    src/elog_utils.c
    src/elog_async.c
    port/elog_port.c)
target_include_directories(kernel PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)
