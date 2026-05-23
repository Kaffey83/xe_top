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


#ifndef MEM_MONITOR_H
#define MEM_MONITOR_H

#include <stdint.h>

/* 内存带宽快照 */
typedef struct
{
    double read_gb_s;
    double write_gb_s;
    double read_mib_s;   /* MiB/s */
    double write_mib_s;  /* MiB/s */
} mem_stats_t;

/* 内存容量快照 */
typedef struct
{
    double total_mib;
    double used_mib;
    double available_mib;
    double usage_pct;
} mem_capacity_t;

/*
 * 通过非核心 IMC 计数器初始化内存带宽监控。
 * 成功返回 0，失败返回 -1。
 */
int mem_monitor_init(void);

/*
 * 读取并计算给定经过时间（秒）内的内存带宽。
 * 内部跟踪之前的原始计数器值以计算增量。
 * 成功返回 0，失败返回 -1。
 */
int mem_monitor_calc(double elapsed_sec, mem_stats_t *stats);

/*
 * 读取系统物理内存容量使用情况。
 * 成功返回 0，失败返回 -1。
 */
int mem_monitor_read_capacity(mem_capacity_t *cap);

/*
 * 清理并关闭所有 IMC perf 事件文件描述符。
 */
void mem_monitor_cleanup(void);

#endif /* MEM_MONITOR_H */