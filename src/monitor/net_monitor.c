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


#include "net_monitor.h"
#include "../util/common.h"
#include "paths.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

/* Cached file descriptors for net stats (opened once in init) */
static int fd_rx = -1;
static int fd_tx = -1;

/* 动态查找活跃的物理网卡 (跳过 lo) */
static int find_active_net(void)
{
    DIR *dir = opendir(SYS_NET_PATH);
    if (!dir)
    {
        return -1;
    }

    char rx_path[512] = {0};
    char tx_path[512] = {0};
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL)
    {
        /* 跳过自身、上级目录和回环接口 */
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0 ||
            strcmp(entry->d_name, "lo") == 0)
        {
            continue;
        }

        char state_path[512];
        snprintf(state_path, sizeof(state_path), "%s/%s/operstate", SYS_NET_PATH, entry->d_name);

        FILE *f = fopen(state_path, "r");
        if (f)
        {
            char state[16] = {0};
            if (fgets(state, sizeof(state), f))
            {
                /* 找到第一个状态为 up 的网卡 */
                if (strncmp(state, "up", 2) == 0)
                {
                    snprintf(rx_path, sizeof(rx_path), "%s/%s/statistics/rx_bytes", SYS_NET_PATH, entry->d_name);
                    snprintf(tx_path, sizeof(tx_path), "%s/%s/statistics/tx_bytes", SYS_NET_PATH, entry->d_name);
                    fclose(f);
                    closedir(dir);

                    fd_rx = open(rx_path, O_RDONLY);
                    fd_tx = open(tx_path, O_RDONLY);
                    return (fd_rx >= 0 && fd_tx >= 0) ? 0 : -1;
                }
            }
            fclose(f);
        }
    }
    closedir(dir);
    return -1;
}

int net_monitor_init(void)
{
    if (find_active_net() != 0)
    {
        return -1;
    }
    return 0;
}

int net_monitor_read(net_stats_t *stats)
{
    if (!stats || fd_rx < 0 || fd_tx < 0)
    {
        return -1;
    }

    /* Read using pread to avoid open/close each cycle */
    char buf[64];

    ssize_t n = pread(fd_rx, buf, sizeof(buf) - 1, 0);
    if (n <= 0) return -1;
    buf[n] = '\0';
    if (sscanf(buf, "%llu", &stats->rx_bytes) != 1) stats->rx_bytes = 0;

    n = pread(fd_tx, buf, sizeof(buf) - 1, 0);
    if (n <= 0) return -1;
    buf[n] = '\0';
    if (sscanf(buf, "%llu", &stats->tx_bytes) != 1) stats->tx_bytes = 0;

    return 0;
}

int net_monitor_compute(const net_stats_t *prev, const net_stats_t *cur, double elapsed_sec, net_metrics_t *out)
{
    if (!prev || !cur || !out || elapsed_sec <= 0.0)
    {
        return -1;
    }

    double delta_rx = (double)DELTA_SAFE(cur->rx_bytes, prev->rx_bytes);
    double delta_tx = (double)DELTA_SAFE(cur->tx_bytes, prev->tx_bytes);

    out->rx_mib_s = (delta_rx / (1024.0 * 1024.0)) / elapsed_sec;
    out->tx_mib_s = (delta_tx / (1024.0 * 1024.0)) / elapsed_sec;
    return 0;
}

void net_monitor_cleanup(void)
{
    if (fd_rx >= 0) close(fd_rx);
    if (fd_tx >= 0) close(fd_tx);
    fd_rx = fd_tx = -1;
}