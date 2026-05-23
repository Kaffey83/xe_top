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


#include "disk_monitor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#define SYS_BLOCK_PATH "/sys/block"

static char disk_stat_path[512] = {0};

/* 动态查找系统主盘 (跳过 loop 设备) */
static int find_main_disk(void)
{
    DIR *dir = opendir(SYS_BLOCK_PATH);
    if (!dir)
    {
        return -1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        /* 跳过自身、上级目录、回环设备和虚拟设备 */
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0 ||
            strncmp(entry->d_name, "loop", 4) == 0 || strncmp(entry->d_name, "zram", 4) == 0)
        {
            continue;
        }

        /* 找到第一个真实盘 (如 nvme0n1, sda) 即可 */
        snprintf(disk_stat_path, sizeof(disk_stat_path), "%s/%s/stat", SYS_BLOCK_PATH, entry->d_name);
        break;
    }
    closedir(dir);
    return (disk_stat_path[0] != '\0') ? 0 : -1;
}

int disk_monitor_init(void)
{
    if (find_main_disk() != 0)
    {
        return -1;
    }
    return 0;
}

int disk_monitor_read(disk_stats_t *stats)
{
    if (!stats || disk_stat_path[0] == '\0')
    {
        return -1;
    }

    FILE *f = fopen(disk_stat_path, "r");
    if (!f)
    {
        return -1;
    }

    /* 
     * /sys/block/nvme0n1/stat 格式 (共11+个字段):
     * Field 1: 读次数
     * Field 2: 合并读次数
     * Field 3: 读扇区数  <-- 我们要的
     * Field 4: 读耗时
     * Field 5: 写次数
     * Field 6: 合并写次数
     * Field 7: 写扇区数  <-- 我们要的
     * 为了避免 %*d 跳过字段在某些系统上遇到前导空格解析失败，
     * 我们将前 7 个字段全部读出。
     */
    unsigned long long f1, f2, read_sectors, f4, f5, f6, write_sectors;
    
    if (fscanf(f, "%llu %llu %llu %llu %llu %llu %llu", 
               &f1, &f2, &read_sectors, &f4, &f5, &f6, &write_sectors) != 7)
    {
        fclose(f);
        return -1;
    }
    fclose(f);

    /* 
     * Linux 内核规定：stat 文件中的扇区数永远是 512 字节为单位，
     * 无论设备的物理/逻辑块大小是多少！所以这里固定乘 512。
     */
    stats->read_bytes = read_sectors * 512;
    stats->write_bytes = write_sectors * 512;
    return 0;
}

int disk_monitor_compute(const disk_stats_t *prev, const disk_stats_t *cur, double elapsed_sec, disk_metrics_t *out)
{
    if (!prev || !cur || !out || elapsed_sec <= 0.0)
    {
        return -1;
    }

    double delta_read = (double)(cur->read_bytes - prev->read_bytes);
    double delta_write = (double)(cur->write_bytes - prev->write_bytes);

    out->read_mib_s = (delta_read / (1024.0 * 1024.0)) / elapsed_sec;
    out->write_mib_s = (delta_write / (1024.0 * 1024.0)) / elapsed_sec;
    return 0;
}

void disk_monitor_cleanup(void)
{
    /* 无需清理 */
}
