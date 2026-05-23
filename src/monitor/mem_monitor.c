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


#include "mem_monitor.h"
#include "../util/perf_util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

static int fd_imc0_r = -1, fd_imc0_w = -1;
static int fd_imc1_r = -1, fd_imc1_w = -1;
static double scale_r = 1.0, scale_w = 1.0;
static unsigned long long prev_raw_r = 0, prev_raw_w = 0;

static double parse_scale(const char *pmu_path, const char *event_name)
{
    char path[256];
    snprintf(path, sizeof(path), "%s/events/%s.scale", pmu_path, event_name);
    FILE *f = fopen(path, "r");
    if (!f)
    {
        return 1.0;
    }
    double scale = 1.0;
    if (fscanf(f, "%lf", &scale) != 1)
    {
        scale = 1.0;
    }
    fclose(f);
    return scale;
}

/*
 * 解析事件文件中的双字段格式: "event=0xff,umask=0x00"
 * 提取 event 和 umask 的值，配合 format 信息，用 perf_util_build_config 拼合 config
 */
static unsigned long long build_imc_config(const char *pmu_path, const char *event_name)
{
    /* 1. 解析 format 位偏移 */
    unsigned long long fmt_event = perf_util_parse_format(pmu_path, "event", 0, 8);
    unsigned long long fmt_umask = perf_util_parse_format(pmu_path, "umask", 8, 8);

    /* 2. 读取事件文件内容 */
    char path[256];
    snprintf(path, sizeof(path), "%s/events/%s", pmu_path, event_name);
    FILE *f = fopen(path, "r");
    if (!f)
    {
        return 0;
    }
    char buf[128];
    if (!fgets(buf, sizeof(buf), f))
    {
        fclose(f);
        return 0;
    }
    fclose(f);

    /* 3. 解析 "event=0xff,umask=0x00" 格式 */
    unsigned long long val_event = 0, val_umask = 0;
    char *p = buf;

    /* 查找 event= */
    char *ev_str = strstr(p, "event=");
    if (ev_str)
    {
        ev_str += 6; /* skip "event=" */
        val_event = strtoull(ev_str, NULL, 16);
    }

    /* 查找 umask= */
    char *um_str = strstr(p, "umask=");
    if (um_str)
    {
        um_str += 6; /* skip "umask=" */
        val_umask = strtoull(um_str, NULL, 16);
    }

    /* 4. 用 perf_util_build_config 组合 */
    unsigned long long fmt_info[2] = { fmt_event, fmt_umask };
    unsigned long long vals[2] = { val_event, val_umask };
    return perf_util_build_config(fmt_info, vals, 2);
}

static int open_imc_event(int pmu_type, unsigned long long config, int cpu)
{
    struct perf_event_attr pea;
    memset(&pea, 0, sizeof(pea));
    pea.size = sizeof(pea);
    pea.type = pmu_type;
    pea.config = config;
    pea.disabled = 0;
    pea.inherit = 1;

    int fd = perf_util_open(&pea, -1, cpu, -1, 0);
    if (fd == -1)
    {
        fprintf(stderr, "MEM: open event (type=%d, config=%llx, cpu=%d) 失败: %s\n", pmu_type, config, cpu, strerror(errno));
    }
    return fd;
}

int mem_monitor_init(void)
{
    const char *imc0_name = "uncore_imc_free_running_0";
    const char *imc1_name = "uncore_imc_free_running_1";
    const char *imc0_path = "/sys/bus/event_source/devices/uncore_imc_free_running_0";

    int type0 = perf_util_get_pmu_type(imc0_name);
    int type1 = perf_util_get_pmu_type(imc1_name);
    if (type0 == -1 || type1 == -1)
    {
        fprintf(stderr, "内存: 无法获取 IMC PMU 类型\n");
        return -1;
    }

    /* 用 build_imc_config 正确解析并组合 event + umask */
    unsigned long long code_r = build_imc_config(imc0_path, "data_read");
    unsigned long long code_w = build_imc_config(imc0_path, "data_write");
    if (!code_r || !code_w)
    {
        fprintf(stderr, "内存: 无法构建 IMC 事件 config (code_r=%llx, code_w=%llx)\n", code_r, code_w);
        return -1;
    }

    scale_r = parse_scale(imc0_path, "data_read");
    scale_w = parse_scale(imc0_path, "data_write");

    int imc_cpu = perf_util_get_first_cpu(imc0_name);
    if (imc_cpu == -1)
    {
        imc_cpu = 0;
    }

    fd_imc0_r = open_imc_event(type0, code_r, imc_cpu);
    fd_imc0_w = open_imc_event(type0, code_w, imc_cpu);
    fd_imc1_r = open_imc_event(type1, code_r, imc_cpu);
    fd_imc1_w = open_imc_event(type1, code_w, imc_cpu);

    if (fd_imc0_r == -1 || fd_imc0_w == -1 || fd_imc1_r == -1 || fd_imc1_w == -1)
    {
        fprintf(stderr, "内存: perf_event_open 失败\n");
        mem_monitor_cleanup();
        return -1;
    }

    unsigned long long dummy;
    ssize_t r0 = read(fd_imc0_r, &dummy, sizeof(dummy));
    ssize_t r1 = read(fd_imc0_w, &dummy, sizeof(dummy));
    ssize_t r2 = read(fd_imc1_r, &dummy, sizeof(dummy));
    ssize_t r3 = read(fd_imc1_w, &dummy, sizeof(dummy));
    (void)r0; (void)r1; (void)r2; (void)r3;
    prev_raw_r = 0;
    prev_raw_w = 0;

    return 0;
}

int mem_monitor_calc(double elapsed_sec, mem_stats_t *stats)
{
    if (!stats || fd_imc0_r == -1 || elapsed_sec <= 0.0)
    {
        return -1;
    }

    unsigned long long cur_r0, cur_w0, cur_r1, cur_w1;
    if (read(fd_imc0_r, &cur_r0, sizeof(cur_r0)) != sizeof(cur_r0)) return -1;
    if (read(fd_imc0_w, &cur_w0, sizeof(cur_w0)) != sizeof(cur_w0)) return -1;
    if (read(fd_imc1_r, &cur_r1, sizeof(cur_r1)) != sizeof(cur_r1)) return -1;
    if (read(fd_imc1_w, &cur_w1, sizeof(cur_w1)) != sizeof(cur_w1)) return -1;

    unsigned long long cur_raw_r = cur_r0 + cur_r1;
    unsigned long long cur_raw_w = cur_w0 + cur_w1;

    long long delta_r = (prev_raw_r == 0) ? 0 : (long long)(cur_raw_r - prev_raw_r);
    long long delta_w = (prev_raw_w == 0) ? 0 : (long long)(cur_raw_w - prev_raw_w);

    if (delta_r < 0)
    {
        delta_r = (long long)cur_raw_r;
    }
    if (delta_w < 0)
    {
        delta_w = (long long)cur_raw_w;
    }

    // (delta * scale) 得到的是 MiB 数量
    stats->read_mib_s = (delta_r * scale_r) / elapsed_sec;
    stats->write_mib_s = (delta_w * scale_w) / elapsed_sec;

    // GB/s (1 GB = 1024 MiB)
    stats->read_gb_s = stats->read_mib_s / 1024.0;
    stats->write_gb_s = stats->write_mib_s / 1024.0;

    prev_raw_r = cur_raw_r;
    prev_raw_w = cur_raw_w;

    return 0;
}

void mem_monitor_cleanup(void)
{
    if (fd_imc0_r != -1) close(fd_imc0_r);
    if (fd_imc0_w != -1) close(fd_imc0_w);
    if (fd_imc1_r != -1) close(fd_imc1_r);
    if (fd_imc1_w != -1) close(fd_imc1_w);

    fd_imc0_r = fd_imc0_w = fd_imc1_r = fd_imc1_w = -1;
    prev_raw_r = prev_raw_w = 0;
}

int mem_monitor_read_capacity(mem_capacity_t *cap)
{
    if (!cap) return -1;

    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) return -1;

    unsigned long long total = 0, available = 0;
    char line[256];

    while (fgets(line, sizeof(line), f))
    {
        if (strncmp(line, "MemTotal:", 9) == 0)
        {
            sscanf(line + 9, "%llu", &total);
        }
        else if (strncmp(line, "MemAvailable:", 13) == 0)
        {
            sscanf(line + 13, "%llu", &available);
        }
    }
    fclose(f);

    if (total == 0) return -1;

    /* 单位从 kB 转换为 MiB */
    cap->total_mib = total / 1024.0;
    cap->available_mib = available / 1024.0;

    /* 真实已用 = 总量 - 可用 (Available 包含了可回收的缓存，最准确) */
    cap->used_mib = cap->total_mib - cap->available_mib;

    cap->usage_pct = (cap->used_mib / cap->total_mib) * 100.0;
    if (cap->usage_pct > 100.0) cap->usage_pct = 100.0;

    return 0;
}
