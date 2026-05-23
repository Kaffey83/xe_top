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


#ifndef GPU_MONITOR_H
#define GPU_MONITOR_H

#include <stdint.h>
#include <stdbool.h>

/* GPU 状态快照 */
typedef struct
{
    unsigned long long active_ticks;
    unsigned long long total_ticks;
    unsigned long long video_active_ticks;
    unsigned long long video_total_ticks;
    unsigned long long freq_accumulator;
    unsigned long long vram_used_bytes; /* Xe 核显 GTT 显存占用 (字节) */
} gpu_stats_t;

/* 由两个快照计算的派生指标 */
typedef struct
{
    double utilization;        /* 渲染引擎 active/total 百分比 (0-100) */
    double video_utilization;  /* 视频引擎占用百分比 (0-100) */
    double frequency_mhz;      /* 派生频率 MHz */
    double vram_used_mib;      /* 核显显存占用 (MiB) */
} gpu_metrics_t;

int gpu_monitor_init(void);
int gpu_monitor_read(gpu_stats_t *stats);
int gpu_monitor_compute(const gpu_stats_t *prev, const gpu_stats_t *cur, double elapsed_sec, gpu_metrics_t *out);
void gpu_monitor_cleanup(void);

#endif /* GPU_MONITOR_H */