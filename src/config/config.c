// SPDX-License-Identifier: GPL-2.0
/*
 * xe_top - Intel Core Ultra Full-Stack Performance Monitoring Tool
 * 
 * Copyright (C) 2025
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>

void config_parse(int argc, char *argv[], runtime_config_t *cfg)
{
    /* Set default values */
    cfg->interval_sec = 1;
    cfg->enable_gpu = true;
    cfg->enable_cpu = true;
    cfg->enable_power = true;
    cfg->enable_mem = true;
    cfg->enable_battery = true;
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
        {"no-battery", no_argument, NULL, 'B'},
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
                cfg->interval_sec = atof(optarg); /* Use atof to parse floating point */
                if (cfg->interval_sec < 0.1)      /* Allow sub-second, but minimum 0.1s (100ms) to prevent lockup */
                {
                    cfg->interval_sec = 0.1;
                }
                break;
            case 'G': cfg->enable_gpu = false; break;
            case 'C': cfg->enable_cpu = false; break;
            case 'P': cfg->enable_power = false; break;
            case 'M': cfg->enable_mem = false; break;
            case 'B': cfg->enable_battery = false; break;
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
    printf("  -i, --interval <秒>  刷新间隔 (默认: 1，支持亚秒级，最小值 0.1)\n");
    printf("  -G, --no-gpu         禁用 GPU 监控\n");
    printf("  -C, --no-cpu         禁用 CPU 监控\n");
    printf("  -P, --no-power       禁用功耗监控\n");
    printf("  -M, --no-memory      禁用内存监控（带宽 + 容量）\n");
    printf("  -B, --no-battery     禁用电池监控\n");
    printf("  -D, --no-disk        禁用磁盘监控\n");
    printf("  -N, --no-net         禁用网络监控\n");
    printf("  -A, --no-npu         禁用 NPU 监控\n");
    printf("  -h, --help           显示此帮助信息\n");
}