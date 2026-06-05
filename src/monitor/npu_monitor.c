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


#include "npu_monitor.h"
#include "../util/common.h"
#include "paths.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

/* Cached file descriptors for NPU sysfs (opened once in init) */
static int fd_busy = -1;
static int fd_freq = -1;
static int fd_max_freq = -1;
static int fd_mem = -1;

static int npu_available = 0;
/* Track whether frequency info is available from driver */
static int npu_freq_available = 0;

/* Read an unsigned long long from a persistent fd using pread */
static int pread_ull(int fd, unsigned long long *out)
{
    if (fd < 0 || !out) return -1;
    char buf[32];
    ssize_t n = pread(fd, buf, sizeof(buf) - 1, 0);
    if (n <= 0) return -1;
    buf[n] = '\0';
    return (sscanf(buf, "%llu", out) == 1) ? 0 : -1;
}

/* Read an unsigned int from a persistent fd using pread */
static int pread_uint(int fd, unsigned int *out)
{
    if (fd < 0 || !out) return -1;
    char buf[32];
    ssize_t n = pread(fd, buf, sizeof(buf) - 1, 0);
    if (n <= 0) return -1;
    buf[n] = '\0';
    return (sscanf(buf, "%u", out) == 1) ? 0 : -1;
}

int npu_monitor_init(void)
{
    char path[256];

    /* Open busy time fd (required) */
    snprintf(path, sizeof(path), "%s/npu_busy_time_us", NPU_DEV_PATH);
    fd_busy = open(path, O_RDONLY);
    if (fd_busy < 0)
    {
        npu_available = 0;
        return -1;
    }

    /* Open frequency fds (optional - driver may not support) */
    snprintf(path, sizeof(path), "%s/npu_current_frequency_mhz", NPU_DEV_PATH);
    fd_freq = open(path, O_RDONLY);

    snprintf(path, sizeof(path), "%s/npu_max_frequency_mhz", NPU_DEV_PATH);
    fd_max_freq = open(path, O_RDONLY);

    npu_freq_available = (fd_freq >= 0 && fd_max_freq >= 0);

    /* Open memory utilization fd (optional) */
    snprintf(path, sizeof(path), "%s/npu_memory_utilization", NPU_DEV_PATH);
    fd_mem = open(path, O_RDONLY);

    npu_available = 1;
    return 0;
}

int npu_monitor_read(npu_stats_t *stats)
{
    if (!stats || !npu_available)
    {
        return -1;
    }

    /* Read busy time (required) */
    if (pread_ull(fd_busy, &stats->busy_time_us) < 0) return -1;

    /* Read frequency (optional, graceful degradation) */
    if (npu_freq_available)
    {
        if (pread_uint(fd_freq, &stats->freq_mhz) < 0) stats->freq_mhz = 0;
        if (pread_uint(fd_max_freq, &stats->max_freq_mhz) < 0) stats->max_freq_mhz = 0;
    }
    else
    {
        stats->freq_mhz = 0;
        stats->max_freq_mhz = 0;
    }

    /* Read memory utilization (optional) */
    if (fd_mem >= 0)
    {
        if (pread_ull(fd_mem, &stats->mem_bytes) < 0) stats->mem_bytes = 0;
    }
    else
    {
        stats->mem_bytes = 0;
    }

    return 0;
}

int npu_monitor_compute(const npu_stats_t *prev, const npu_stats_t *cur, double elapsed_sec, npu_metrics_t *out)
{
    if (!prev || !cur || !out || elapsed_sec <= 0.0)
    {
        return -1;
    }

    /* 计算占用率: (忙碌时间差 / 总流逝时间) * 100% */
    long long delta_busy = DELTA_SAFE(cur->busy_time_us, prev->busy_time_us);

    double total_elapsed_us = elapsed_sec * 1000000.0;
    out->utilization_pct = (delta_busy / total_elapsed_us) * 100.0;
    
    if (out->utilization_pct > 100.0)
    {
        out->utilization_pct = 100.0;
    }

    out->freq_mhz = (double)cur->freq_mhz;
    out->max_freq_mhz = (double)cur->max_freq_mhz;
    out->mem_mib = (double)cur->mem_bytes / (1024.0 * 1024.0);

    return 0;
}

void npu_monitor_cleanup(void)
{
    if (fd_busy >= 0) close(fd_busy);
    if (fd_freq >= 0) close(fd_freq);
    if (fd_max_freq >= 0) close(fd_max_freq);
    if (fd_mem >= 0) close(fd_mem);
    fd_busy = fd_freq = fd_max_freq = fd_mem = -1;
    npu_available = 0;
    npu_freq_available = 0;
}