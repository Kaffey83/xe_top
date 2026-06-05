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


#include "gpu_monitor.h"
#include "../util/perf_util.h"
#include "../util/common.h"
#include "paths.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <ctype.h>

/* 动态查找的 Xe PMU 路径和类型 */
static char xe_pmu_path[512] = {0};
static int xe_pmu_type = -1;

/* 各事件文件描述符 */
static int fd_active = -1;
static int fd_total = -1;
static int fd_video_active = -1;
static int fd_video_total = -1;
static int fd_freq = -1;

/* Xe GPU 配置字段格式信息 (动态解析) */
static unsigned long long fmt_event;
static unsigned long long fmt_class;
static unsigned long long fmt_inst;
static unsigned long long fmt_gt;

/* GTT 显存扫描优化 */
#define GTT_SCAN_INTERVAL 3 /* 每 3 个采样周期扫描一次显存 */
static int gtt_scan_counter = 0;
static unsigned long long last_gtt_bytes = 0;

/* 扫描 /sys/bus/event_source/devices/ 动态查找 Xe PMU 设备 */
static int find_xe_pmu(void)
{
    DIR *dir = opendir("/sys/bus/event_source/devices");
    if (!dir)
    {
        return -1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (strncmp(entry->d_name, "xe_", 3) == 0)
        {
            snprintf(xe_pmu_path, sizeof(xe_pmu_path), "/sys/bus/event_source/devices/%s", entry->d_name);
            xe_pmu_type = perf_util_get_pmu_type(entry->d_name);
            break;
        }
    }
    closedir(dir);

    return (xe_pmu_type != -1) ? 0 : -1;
}

/* 动态解析 Xe PMU 各字段的位偏移与长度 */
static int parse_xe_format(void)
{
    fmt_event = perf_util_parse_format(xe_pmu_path, "event", 0, 12);
    fmt_class = perf_util_parse_format(xe_pmu_path, "engine_class", 20, 8);
    fmt_inst = perf_util_parse_format(xe_pmu_path, "engine_instance", 12, 8);
    fmt_gt = perf_util_parse_format(xe_pmu_path, "gt", 60, 4);
    return 0;
}

/* 打开指定配置的 Xe PMU 事件 */
static int open_xe_event(unsigned long long config)
{
    return perf_util_open_event(xe_pmu_type, config, xe_pmu_path);
}

/* 去重用的 client-id 记录，最多 256 个 client */
#define MAX_XE_CLIENTS 256
static unsigned int seen_clients[MAX_XE_CLIENTS];
static int seen_client_count = 0;

static int is_client_seen(unsigned int client_id)
{
    for (int i = 0; i < seen_client_count; i++)
    {
        if (seen_clients[i] == client_id) return 1;
    }
    return 0;
}

static void mark_client_seen(unsigned int client_id)
{
    if (seen_client_count < MAX_XE_CLIENTS)
    {
        seen_clients[seen_client_count++] = client_id;
    }
}

/* 遍历 /proc 下所有进程，按 drm-client-id 去重后累加 Xe 驱动的 drm-resident-gtt */
static unsigned long long read_xe_gtt_mem(void)
{
    /* 优化：降低扫描频率，避免高频遍历 /proc 导致 CPU 开销过大 */
    if (++gtt_scan_counter < GTT_SCAN_INTERVAL)
    {
        return last_gtt_bytes;
    }
    gtt_scan_counter = 0;

    DIR *proc_dir = opendir("/proc");
    if (!proc_dir) return last_gtt_bytes;

    unsigned long long total_gtt_kib = 0;
    struct dirent *proc_entry;

    /* 每次扫描前清空去重表 */
    seen_client_count = 0;

    while ((proc_entry = readdir(proc_dir)) != NULL)
    {
        /* 只遍历数字目录 (PID) */
        if (!isdigit(proc_entry->d_name[0])) continue;

        char fdinfo_path[512];
        snprintf(fdinfo_path, sizeof(fdinfo_path), "/proc/%s/fdinfo", proc_entry->d_name);

        DIR *fdinfo_dir = opendir(fdinfo_path);
        if (!fdinfo_dir) continue;

        struct dirent *fd_entry;
        while ((fd_entry = readdir(fdinfo_dir)) != NULL)
        {
            char file_path[1024];
            snprintf(file_path, sizeof(file_path), "%s/%s", fdinfo_path, fd_entry->d_name);

            FILE *f = fopen(file_path, "r");
            if (!f) continue;

            char line[256];
            int is_xe = 0;
            unsigned long long gtt_kib = 0;
            unsigned int client_id = 0;
            int found_gtt = 0;
            int found_client = 0;

            while (fgets(line, sizeof(line), f))
            {
                if (strncmp(line, "drm-driver:\txe", 14) == 0)
                {
                    is_xe = 1;
                }
                else if (strncmp(line, "drm-client-id:\t", 15) == 0)
                {
                    sscanf(line + 15, "%u", &client_id);
                    found_client = 1;
                }
                else if (strncmp(line, "drm-resident-gtt:\t", 18) == 0)
                {
                    sscanf(line + 18, "%llu", &gtt_kib);
                    found_gtt = 1;
                }
            }
            fclose(f);

            /* 必须是 xe 驱动，且同时找到 client-id 和 gtt 数据 */
            if (is_xe && found_client && found_gtt)
            {
                /* 按 client-id 去重 */
                if (!is_client_seen(client_id))
                {
                    mark_client_seen(client_id);
                    total_gtt_kib += gtt_kib;
                }
            }
        }
        closedir(fdinfo_dir);
    }
    closedir(proc_dir);

    /* KiB 转换为 Bytes 并缓存 */
    last_gtt_bytes = total_gtt_kib * 1024;
    return last_gtt_bytes;
}

int gpu_monitor_init(void)
{
    if (find_xe_pmu() != 0)
    {
        return -1;
    }

    parse_xe_format();

    unsigned long long active_code = perf_util_parse_event_code(xe_pmu_path, "engine-active-ticks");
    unsigned long long total_code = perf_util_parse_event_code(xe_pmu_path, "engine-total-ticks");
    unsigned long long freq_code = perf_util_parse_event_code(xe_pmu_path, "gt-actual-frequency");

    if (!active_code || !total_code || !freq_code)
    {
        gpu_monitor_cleanup();
        return -1;
    }

    /* 通用格式数组：[event, instance, class] */
    unsigned long long fmt_arr[3] = {fmt_event, fmt_inst, fmt_class};

    /* 渲染引擎 (engine_class = 0, engine_instance = 0) */
    unsigned long long vals_active[3] = {active_code, 0, 0};
    unsigned long long vals_total[3] = {total_code, 0, 0};
    unsigned long long active_config = perf_util_build_config(fmt_arr, vals_active, 3);
    unsigned long long total_config = perf_util_build_config(fmt_arr, vals_total, 3);

    /* 视频解码引擎 (engine_class = 2, engine_instance = 0) */
    unsigned long long vals_video_active[3] = {active_code, 0, 2};
    unsigned long long vals_video_total[3] = {total_code, 0, 2};
    unsigned long long video_active_config = perf_util_build_config(fmt_arr, vals_video_active, 3);
    unsigned long long video_total_config = perf_util_build_config(fmt_arr, vals_video_total, 3);

    /* 全局 GT 事件 (只需 event 码) */
    unsigned long long fmt_freq[1] = {fmt_event};
    unsigned long long vals_freq[1] = {freq_code};
    unsigned long long freq_config = perf_util_build_config(fmt_freq, vals_freq, 1);

    fd_active = open_xe_event(active_config);
    fd_total = open_xe_event(total_config);
    fd_video_active = open_xe_event(video_active_config);
    fd_video_total = open_xe_event(video_total_config);
    fd_freq = open_xe_event(freq_config);

    if (fd_active == -1 || fd_total == -1 || fd_freq == -1)
    {
        gpu_monitor_cleanup();
        return -1;
    }

    return 0;
}

int gpu_monitor_read(gpu_stats_t *stats)
{
    if (!stats || fd_active == -1)
    {
        return -1;
    }

    memset(stats, 0, sizeof(*stats));

    if (perf_util_read_counter(fd_active, &stats->active_ticks) < 0 ||
        perf_util_read_counter(fd_total, &stats->total_ticks) < 0 ||
        perf_util_read_counter(fd_freq, &stats->freq_accumulator) < 0)
    {
        return -1;
    }

    /* 读取 Video 引擎 (失败不阻断整体逻辑) */
    if (fd_video_active >= 0)
    {
        perf_util_read_counter(fd_video_active, &stats->video_active_ticks);
    }
    if (fd_video_total >= 0)
    {
        perf_util_read_counter(fd_video_total, &stats->video_total_ticks);
    }

    /* 读取 Xe 核显 GTT 显存占用 */
    stats->vram_used_bytes = read_xe_gtt_mem();

    return 0;
}

int gpu_monitor_compute(const gpu_stats_t *prev, const gpu_stats_t *cur, double elapsed_sec, gpu_metrics_t *out)
{
    if (!prev || !cur || !out || elapsed_sec <= 0.0)
    {
        return -1;
    }

    /* 计算增量 (DELTA_SAFE protects against counter wrap-around) */
    long long delta_active = DELTA_SAFE(cur->active_ticks, prev->active_ticks);
    long long delta_total = DELTA_SAFE(cur->total_ticks, prev->total_ticks);
    long long delta_video_active = DELTA_SAFE(cur->video_active_ticks, prev->video_active_ticks);
    long long delta_video_total = DELTA_SAFE(cur->video_total_ticks, prev->video_total_ticks);
    long long delta_freq = DELTA_SAFE(cur->freq_accumulator, prev->freq_accumulator);

    /* 标记视频引擎是否可用 */
    out->video_available = (fd_video_active >= 0 && fd_video_total >= 0);

    /* --- 渲染引擎利用率 --- */
    if (delta_total > 0 && prev->total_ticks > 0)
    {
        out->utilization = (double)delta_active / (double)delta_total * 100.0;
        if (out->utilization > 100.0)
        {
            out->utilization = 100.0;
        }
    }
    else
    {
        out->utilization = 0.0;
    }

    /* --- 视频引擎利用率 --- */
    if (delta_video_total > 0 && prev->video_total_ticks > 0)
    {
        out->video_utilization = (double)delta_video_active / (double)delta_video_total * 100.0;
        if (out->video_utilization > 100.0)
        {
            out->video_utilization = 100.0;
        }
    }
    else
    {
        out->video_utilization = 0.0;
    }

    /* --- 实时频率 --- */
    if (elapsed_sec > 0.0)
    {
        out->frequency_mhz = (double)delta_freq / elapsed_sec;
    }
    else
    {
        out->frequency_mhz = 0.0;
    }

    /* --- 核显显存占用 (直接读取当前值，非差值) --- */
    out->vram_used_mib = (double)cur->vram_used_bytes / (1024.0 * 1024.0);

    return 0;
}

void gpu_monitor_cleanup(void)
{
    if (fd_active != -1) close(fd_active);
    if (fd_total != -1) close(fd_total);
    if (fd_video_active != -1) close(fd_video_active);
    if (fd_video_total != -1) close(fd_video_total);
    if (fd_freq != -1) close(fd_freq);

    fd_active = fd_total = fd_freq = -1;
    fd_video_active = fd_video_total = -1;
}
