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
#include "../util/common.h"
#include "paths.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

/* Cached file descriptors for RAPL sysfs (opened once in init) */
static int fd_pkg = -1;
static int fd_core = -1;
static int fd_dram = -1;

/* Read an unsigned long long from a persistent fd using pread */
static unsigned long long read_energy_uj(int fd)
{
    if (fd < 0)
    {
        return 0;
    }
    char buf[32];
    ssize_t n = pread(fd, buf, sizeof(buf) - 1, 0);
    if (n <= 0)
    {
        return 0;
    }
    buf[n] = '\0';
    unsigned long long val = 0;
    sscanf(buf, "%llu", &val);
    return val;
}

/* Initialize power monitor: open RAPL sysfs file descriptors */
int power_monitor_init(void)
{
    fd_pkg = open(RAPL_PKG_PATH, O_RDONLY);
    fd_core = open(RAPL_CORE_PATH, O_RDONLY);
    fd_dram = open(RAPL_DRAM_PATH, O_RDONLY);

    if (fd_pkg < 0)
    {
        fprintf(stderr, "Power: insufficient permissions (run with sudo)\n");
        power_monitor_cleanup();
        return -1;
    }

    return 0;
}

/* Read current power stats (using cached fd + pread) */
int power_monitor_read(power_stats_t *stats)
{
    if (!stats)
    {
        return -1;
    }
    stats->pkg_energy = read_energy_uj(fd_pkg);
    stats->core_energy = read_energy_uj(fd_core);
    stats->dram_energy = read_energy_uj(fd_dram);
    return 0;
}

/* Compute average power from energy delta (unit: watts) */
int power_monitor_compute(const power_stats_t *prev, const power_stats_t *cur, double elapsed_sec, power_metrics_t *out)
{
    if (!prev || !cur || !out || elapsed_sec <= 0.0)
    {
        return -1;
    }

    /* Energy delta (microjoules), DELTA_SAFE protects against counter wrap-around */
    long long delta_pkg = DELTA_SAFE(cur->pkg_energy, prev->pkg_energy);
    long long delta_core = DELTA_SAFE(cur->core_energy, prev->core_energy);
    long long delta_dram = DELTA_SAFE(cur->dram_energy, prev->dram_energy);

    out->pkg_watts = (double)delta_pkg / elapsed_sec / 1000000.0;
    out->core_watts = (double)delta_core / elapsed_sec / 1000000.0;
    out->dram_watts = (double)delta_dram / elapsed_sec / 1000000.0;

    return 0;
}

/* Power monitor cleanup: close cached fds */
void power_monitor_cleanup(void)
{
    if (fd_pkg >= 0) close(fd_pkg);
    if (fd_core >= 0) close(fd_core);
    if (fd_dram >= 0) close(fd_dram);
    fd_pkg = fd_core = fd_dram = -1;
}