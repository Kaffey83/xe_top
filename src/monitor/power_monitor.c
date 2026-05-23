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


#include "power_monitor.h"
#include <stdio.h>
#include <stdlib.h>

#define PKG_PATH "/sys/class/powercap/intel-rapl:0/energy_uj"
#define CORE_PATH "/sys/class/powercap/intel-rapl:0:0/energy_uj"
#define DRAM_PATH "/sys/class/powercap/intel-rapl:0:1/energy_uj"

/* 读取指定路径的能量值（单位：微焦耳） */
static unsigned long long read_energy_uj(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f)
    {
        return 0;
    }
    unsigned long long val = 0;
    if (fscanf(f, "%llu", &val) != 1)
    {
        val = 0;
    }
    fclose(f);
    return val;
}

/* 初始化功耗监控：检查 sysfs 路径的读取权限 */
int power_monitor_init(void)
{
    FILE *f = fopen(PKG_PATH, "r");
    if (!f)
    {
        fprintf(stderr, "功耗: 权限不足 (请使用 sudo 运行)\n");
        return -1;
    }
    fclose(f);
    return 0;
}

/* 读取当前功耗统计信息 */
int power_monitor_read(power_stats_t *stats)
{
    if (!stats)
    {
        return -1;
    }
    stats->pkg_energy = read_energy_uj(PKG_PATH);
    stats->core_energy = read_energy_uj(CORE_PATH);
    stats->dram_energy = read_energy_uj(DRAM_PATH);
    return 0;
}

/* 根据两次采样的能量差值计算平均功耗（单位：瓦特） */
int power_monitor_compute(const power_stats_t *prev, const power_stats_t *cur, double elapsed_sec, power_metrics_t *out)
{
    if (!prev || !cur || !out || elapsed_sec <= 0.0)
    {
        return -1;
    }

    /* 能量差值（微焦耳），除以时间（秒）得到微瓦，再除以 1e6 得到瓦特 */
    long long delta_pkg = (long long)(cur->pkg_energy - prev->pkg_energy);
    long long delta_core = (long long)(cur->core_energy - prev->core_energy);
    long long delta_dram = (long long)(cur->dram_energy - prev->dram_energy);

    /* 防御无符号减法下溢 (计数器翻转或异常重置) */
    if (delta_pkg < 0) delta_pkg = 0;
    if (delta_core < 0) delta_core = 0;
    if (delta_dram < 0) delta_dram = 0;

    out->pkg_watts = (double)delta_pkg / elapsed_sec / 1000000.0;
    out->core_watts = (double)delta_core / elapsed_sec / 1000000.0;
    out->dram_watts = (double)delta_dram / elapsed_sec / 1000000.0;

    return 0;
}

/* 功耗监控清理 */
void power_monitor_cleanup(void)
{
    /* （sysfs 读取无需清理操作） */
}
