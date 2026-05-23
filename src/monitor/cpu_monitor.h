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


#ifndef CPU_MONITOR_H
#define CPU_MONITOR_H

#include <stdint.h>

/* 架构特定的单核心类型 CPU 状态 */
typedef struct
{
    unsigned long long instructions;
    unsigned long long cycles;
    unsigned long long ref_cycles;
    unsigned long long cache_refs;
    unsigned long long cache_misses;
    unsigned long long branch_misses;
    unsigned long long branch_instructions;
    unsigned long long aperf;          /* 实际性能时钟计数器 */
    unsigned long long mperf;          /* 最大性能时钟计数器 */
    unsigned long long c6_residency;   /* Core C6 驻留时间 */
    unsigned long long thermal_margin; /* 降频前的热裕度 */
} cpu_arch_stats_t;

/* 完整 CPU 状态快照（P-Core + E-Core）*/
typedef struct
{
    cpu_arch_stats_t pcore;  /* P-Core（性能核心）*/
    cpu_arch_stats_t ecore;  /* E-Core（效率核心）*/
    unsigned long long pkg_c10_residency; /* Package C10 驻留时间 */
} cpu_stats_t;

/* 派生 CPU 指标 */
typedef struct
{
    double pcore_freq_mhz;        /* P-Core 实际频率 MHz */
    double ecore_freq_mhz;        /* E-Core 实际频率 MHz */
    double pcore_ipc;             /* P-Core 每周期指令数 */
    double ecore_ipc;             /* E-Core 每周期指令数 */
    double pcore_cache_miss_rate; /* P-Core 缓存未命中率 % */
    double ecore_cache_miss_rate; /* E-Core 缓存未命中率 % */
    double pcore_branch_miss_rate;/* P-Core 分支未命中率 % */
    double ecore_branch_miss_rate;/* E-Core 分支未命中率 % */
    double core_c6_pct;           /* Core C6 驻留百分比 % */
    double pkg_c10_pct;           /* Package C10 驻留百分比 % */
    int thermal_margin_degrees;   /* 降频前的摄氏度 °C */
} cpu_metrics_t;

/*
 * 初始化 CPU PMU 监控子系统。
 * 为 P-Core 和 E-Core 打开 perf_events。
 * 成功返回 0，失败返回 -1。
 */
int cpu_monitor_init(void);

/*
 * 读取当前原始 CPU 状态快照。
 * 成功返回 0，失败返回 -1。
 */
int cpu_monitor_read(cpu_stats_t *stats);

/*
 * 从两个连续快照计算派生指标。
 * 成功返回 0，无效输入返回 -1。
 */
int cpu_monitor_compute(const cpu_stats_t *prev, const cpu_stats_t *cur,
                        cpu_metrics_t *out);

/*
 * 清理并关闭所有 CPU PMU 资源。
 */
void cpu_monitor_cleanup(void);

#endif /* CPU_MONITOR_H */