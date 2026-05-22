cmake_minimum_required(VERSION 3.10)
project(XeTop C)

set(CMAKE_C_STANDARD 11)
if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()
set(CMAKE_C_STANDARD_REQUIRED True)

add_executable(xe_top
    src/main.c
    src/util/perf_util.c
    src/monitor/gpu_monitor.c
    src/monitor/cpu_monitor.c
    src/monitor/power_monitor.c
    src/monitor/mem_monitor.c
    src/config/config.c
    src/display/display.c
    src/monitor/battery_monitor.c
    src/monitor/disk_monitor.c
    src/monitor/net_monitor.c
    src/monitor/npu_monitor.c
)

target_link_libraries(xe_top rt m)