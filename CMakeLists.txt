cmake_minimum_required(VERSION 3.10)
project(XeTop VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

# Compiler warnings
if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# Hardening flags
if (CMAKE_C_COMPILER_ID MATCHES "GNU")
    add_compile_options(-D_FORTIFY_SOURCE=2 -fstack-protector-strong)
endif()

# Sanitizer options (off by default)
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)

if(ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

if(ENABLE_UBSAN)
    add_compile_options(-fsanitize=undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=undefined)
endif()

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

# Install rules
include(GNUInstallDirs)
install(TARGETS xe_top RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES README.md README.zh.md DESTINATION ${CMAKE_INSTALL_DOCDIR})
install(FILES docs/xe_top.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)

# Test targets
enable_testing()

add_executable(test_common tests/test_common.c)
target_link_libraries(test_common m)
add_test(NAME test_common COMMAND test_common)

add_executable(test_ring tests/test_ring.c)
target_link_libraries(test_ring m)
add_test(NAME test_ring COMMAND test_ring)

# CPack packaging
include(CPack)
set(CPACK_GENERATOR "DEB;RPM;TGZ")
set(CPACK_PACKAGE_NAME "xe_top")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Intel Core Ultra Full-Stack Performance Monitoring Tool")
set(CPACK_PACKAGE_LICENSE "GPL-2.0")
set(CPACK_PACKAGE_CONTACT "xe_top developers")