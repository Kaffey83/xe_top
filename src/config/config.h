#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>

/* 从命令行参数解析的运行时配置 */
typedef struct
{
    double interval_sec;       /* 刷新间隔 (默认: 1) */
    bool enable_gpu;        /* 启用 GPU 监控 (默认: true) */
    bool enable_cpu;        /* 启用 CPU 监控 (默认: true) */
    bool enable_power;      /* 启用功耗监控 (默认: true) */
    bool enable_mem;        /* 启用内存监控 (默认: true) */
    bool enable_battery;    /* 启用电池监控 (默认: true) */ // 新增
    bool enable_disk;       /* 启用磁盘监控 (默认: true) */
    bool enable_net;        /* 启用网络监控 (默认: true) */
    bool enable_npu;        /* 启用 NPU 监控 (默认: true) */
    bool help_requested;    /* 已请求 --help */
} runtime_config_t;

void config_parse(int argc, char *argv[], runtime_config_t *cfg);
void config_print_usage(const char *prog_name);

#endif /* CONFIG_H */