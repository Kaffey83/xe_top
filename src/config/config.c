#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>

void config_parse(int argc, char *argv[], runtime_config_t *cfg)
{
    /* 设置默认值 */
    cfg->interval_sec = 1;
    cfg->enable_gpu = true;
    cfg->enable_cpu = true;
    cfg->enable_power = true;
    cfg->enable_mem = true;
    cfg->enable_battery = true; // 新增
    cfg->enable_disk = true;
    cfg->enable_net = true;
    cfg->enable_npu = true;
    cfg->help_requested = false;

    static struct option long_opts[] = {
        {"interval", required_argument, NULL, 'i'},
        {"no-gpu", no_argument, NULL, 'G'},
        {"no-cpu", no_argument, NULL, 'C'},
        {"no-power", no_argument, NULL, 'P'},
        {"no-memory", no_argument, NULL, 'M'},
        {"no-battery", no_argument, NULL, 'B'}, // 新增
        {"no-disk", no_argument, NULL, 'D'},
        {"no-net", no_argument, NULL, 'N'},
        {"no-npu", no_argument, NULL, 'A'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "i:GCPhMBDNA", long_opts, NULL)) != -1)
    {
        switch (opt)
        {
            case 'i':
                cfg->interval_sec = atof(optarg); // 改用 atof 解析浮点数
                if (cfg->interval_sec < 0.1)      // 允许亚秒级，但最低限制 0.1 秒 (100ms) 防止刷新太快卡死
                {
                    cfg->interval_sec = 0.1;
                }
                break;
            case 'G': cfg->enable_gpu = false; break;
            case 'C': cfg->enable_cpu = false; break;
            case 'P': cfg->enable_power = false; break;
            case 'M': cfg->enable_mem = false; break;
            case 'B': cfg->enable_battery = false; break; // 新增
            case 'D': cfg->enable_disk = false; break;
            case 'N': cfg->enable_net = false; break;
            case 'A': cfg->enable_npu = false; break;
            case 'h':
                cfg->help_requested = true;
                config_print_usage(argv[0] ? argv[0] : "xe_top");
                return;
            default:
                cfg->help_requested = true;
                config_print_usage(argv[0] ? argv[0] : "xe_top");
                return;
        }
    }
}

void config_print_usage(const char *prog_name)
{
    printf("用法: %s [选项]\n\n", prog_name);
    printf("选项:\n");
    printf("  -i, --interval <秒>  刷新间隔 (默认: 1)\n");
    printf("  -G, --no-gpu         禁用 GPU 监控\n");
    printf("  -C, --no-cpu         禁用 CPU 监控\n");
    printf("  -P, --no-power       禁用功耗监控\n");
    printf("  -M, --no-memory      禁用内存带宽监控\n");
    printf("  -B, --no-battery     禁用电池监控\n"); // 新增
    printf("  -D, --no-disk        禁用磁盘监控\n");
    printf("  -N, --no-net         禁用网络监控\n");
    printf("  -A, --no-npu         禁用 NPU 监控\n");
    printf("  -h, --help           显示此帮助信息\n");
}