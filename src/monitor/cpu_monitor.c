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


#include "cpu_monitor.h"
#include "../util/perf_util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

/* 架构特定事件代码 (带 umask 的事件必须硬编码，防止解析丢失掩码) */
#define EVT_CACHE_REFS  0x4f2e
#define EVT_CACHE_MISSES 0x412e

/* MSR 事件用于频率 */
#define EVT_APERF 0x01
#define EVT_MPERF 0x02

static double tsc_freq_mhz = 2400.0; /* 默认值 */

/* P-Core 文件描述符 */
static int fd_p_inst = -1, fd_p_cyc = -1, fd_p_ref = -1;
static int fd_p_cache_ref = -1, fd_p_cache_miss = -1;
static int fd_p_br_instr = -1, fd_p_br_miss = -1;
static int fd_p_aperf = -1, fd_p_mperf = -1, fd_p_c6 = -1;

/* E-Core 文件描述符 */
static int fd_e_inst = -1, fd_e_cyc = -1, fd_e_ref = -1;
static int fd_e_cache_ref = -1, fd_e_cache_miss = -1;
static int fd_e_br_instr = -1, fd_e_br_miss = -1;
static int fd_e_aperf = -1, fd_e_mperf = -1;

/* Package 文件描述符 */
static int fd_pkg_c10 = -1;

/* 热裕度 */
static int fd_thermal = -1;

static void read_tsc_freq(void)
{
    FILE *f = fopen("/sys/devices/system/cpu/cpu0/tsc_freq_khz", "r");
    if (!f)
    {
        return;
    }
    unsigned long long val = 0;
    if (fscanf(f, "%llu", &val) == 1)
    {
        tsc_freq_mhz = (double)val / 1000.0;
    }
    fclose(f);
}

static int open_cpu_event(int pmu_type, unsigned long long config, int cpu)
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
        fprintf(stderr, "CPU: open event (type=%d, config=%llx, cpu=%d) 失败: %s\n", pmu_type, config, cpu, strerror(errno));
    }
    return fd;
}

int cpu_monitor_init(void)
{
    read_tsc_freq();

    int type_core = perf_util_get_pmu_type("cpu_core");
    int type_atom = perf_util_get_pmu_type("cpu_atom");
    int type_msr = perf_util_get_pmu_type("msr");
    int type_cstate_core = perf_util_get_pmu_type("cstate_core");
    int type_cstate_pkg = perf_util_get_pmu_type("cstate_pkg");

    int p_cpu = perf_util_get_first_cpu("cpu_core");
    int e_cpu = perf_util_get_first_cpu("cpu_atom");

    if (type_core == -1 || type_atom == -1 || p_cpu == -1 || e_cpu == -1)
    {
        fprintf(stderr, "CPU: 无法获取 P-Core/E-Core PMU 类型或 CPU 编号\n");
        cpu_monitor_cleanup();
        return -1;
    }

    /* P-Core 架构事件 */
    fd_p_inst = open_cpu_event(type_core, perf_util_parse_event_code("/sys/bus/event_source/devices/cpu_core", "instructions"), p_cpu);
    fd_p_cyc = open_cpu_event(type_core, perf_util_parse_event_code("/sys/bus/event_source/devices/cpu_core", "cpu-cycles"), p_cpu);
    fd_p_ref = open_cpu_event(type_core, perf_util_parse_event_code("/sys/bus/event_source/devices/cpu_core", "ref-cycles"), p_cpu);
    fd_p_cache_ref = open_cpu_event(type_core, EVT_CACHE_REFS, p_cpu);
    fd_p_cache_miss = open_cpu_event(type_core, EVT_CACHE_MISSES, p_cpu);
    fd_p_br_instr = open_cpu_event(type_core, perf_util_parse_event_code("/sys/bus/event_source/devices/cpu_core", "branch-instructions"), p_cpu);
    fd_p_br_miss = open_cpu_event(type_core, perf_util_parse_event_code("/sys/bus/event_source/devices/cpu_core", "branch-misses"), p_cpu);

    /* E-Core 架构事件 */
    fd_e_inst = open_cpu_event(type_atom, perf_util_parse_event_code("/sys/bus/event_source/devices/cpu_atom", "instructions"), e_cpu);
    fd_e_cyc = open_cpu_event(type_atom, perf_util_parse_event_code("/sys/bus/event_source/devices/cpu_atom", "cpu-cycles"), e_cpu);
    fd_e_ref = open_cpu_event(type_atom, perf_util_parse_event_code("/sys/bus/event_source/devices/cpu_atom", "ref-cycles"), e_cpu);
    fd_e_cache_ref = open_cpu_event(type_atom, EVT_CACHE_REFS, e_cpu);
    fd_e_cache_miss = open_cpu_event(type_atom, EVT_CACHE_MISSES, e_cpu);
    fd_e_br_instr = open_cpu_event(type_atom, perf_util_parse_event_code("/sys/bus/event_source/devices/cpu_atom", "branch-instructions"), e_cpu);
    fd_e_br_miss = open_cpu_event(type_atom, perf_util_parse_event_code("/sys/bus/event_source/devices/cpu_atom", "branch-misses"), e_cpu);

    /* MSR 事件用于频率 */
    if (type_msr != -1)
    {
        fd_p_aperf = open_cpu_event(type_msr, EVT_APERF, p_cpu);
        fd_p_mperf = open_cpu_event(type_msr, EVT_MPERF, p_cpu);
        fd_e_aperf = open_cpu_event(type_msr, EVT_APERF, e_cpu);
        fd_e_mperf = open_cpu_event(type_msr, EVT_MPERF, e_cpu);
    }

    /* cstate_core 事件用于 C6 驻留 */
    if (type_cstate_core != -1)
    {
        unsigned long long c6_code = perf_util_parse_event_code(
            "/sys/bus/event_source/devices/cstate_core", "c6-residency");
        if (c6_code)
        {
            fd_p_c6 = open_cpu_event(type_cstate_core, c6_code, p_cpu);
        }
    }

    /* cstate_pkg 事件用于 C10 驻留 */
    if (type_cstate_pkg != -1)
    {
        unsigned long long c10_code = perf_util_parse_event_code(
            "/sys/bus/event_source/devices/cstate_pkg", "c10-residency");
        if (c10_code)
        {
            fd_pkg_c10 = open_cpu_event(type_cstate_pkg, c10_code, p_cpu);
        }
    }

    /* 通过 MSR 读取热裕度 */
    if (type_msr != -1)
    {
        unsigned long long code_thermal = perf_util_parse_event_code(
            "/sys/bus/event_source/devices/msr", "cpu_thermal_margin");
        if (code_thermal)
        {
            fd_thermal = open_cpu_event(type_msr, code_thermal, p_cpu);
        }
    }

    if (fd_p_inst == -1 || fd_e_inst == -1)
    {
        fprintf(stderr, "CPU: perf_event_open 初始化失败\n");
        cpu_monitor_cleanup();
        return -1;
    }

    return 0;
}

static void read_arch_stats(int fd_inst, int fd_cyc, int fd_ref, int fd_cache_ref, int fd_cache_miss,
                            int fd_br_instr, int fd_br_miss, int fd_aperf, int fd_mperf, int fd_c6,
                            cpu_arch_stats_t *stats)
{
    if (!stats)
    {
        return;
    }

    long long val = 0;

    if (fd_inst >= 0)
    {
        if (read(fd_inst, &val, sizeof(val)) != sizeof(val)) val = 0;
        stats->instructions = (unsigned long long)val;
    }
    if (fd_cyc >= 0)
    {
        if (read(fd_cyc, &val, sizeof(val)) != sizeof(val)) val = 0;
        stats->cycles = (unsigned long long)val;
    }
    if (fd_ref >= 0)
    {
        if (read(fd_ref, &val, sizeof(val)) != sizeof(val)) val = 0;
        stats->ref_cycles = (unsigned long long)val;
    }
    if (fd_cache_ref >= 0)
    {
        if (read(fd_cache_ref, &val, sizeof(val)) != sizeof(val)) val = 0;
        stats->cache_refs = (unsigned long long)val;
    }
    if (fd_cache_miss >= 0)
    {
        if (read(fd_cache_miss, &val, sizeof(val)) != sizeof(val)) val = 0;
        stats->cache_misses = (unsigned long long)val;
    }
    if (fd_br_instr >= 0)
    {
        if (read(fd_br_instr, &val, sizeof(val)) != sizeof(val)) val = 0;
        stats->branch_instructions = (unsigned long long)val;
    }
    if (fd_br_miss >= 0)
    {
        if (read(fd_br_miss, &val, sizeof(val)) != sizeof(val)) val = 0;
        stats->branch_misses = (unsigned long long)val;
    }
    if (fd_aperf >= 0)
    {
        if (read(fd_aperf, &val, sizeof(val)) != sizeof(val)) val = 0;
        stats->aperf = (unsigned long long)val;
    }
    if (fd_mperf >= 0)
    {
        if (read(fd_mperf, &val, sizeof(val)) != sizeof(val)) val = 0;
        stats->mperf = (unsigned long long)val;
    }
    if (fd_c6 >= 0)
    {
        if (read(fd_c6, &val, sizeof(val)) != sizeof(val)) val = 0;
        stats->c6_residency = (unsigned long long)val;
    }
}

int cpu_monitor_read(cpu_stats_t *stats)
{
    if (!stats || fd_p_inst == -1)
    {
        return -1;
    }

    read_arch_stats(fd_p_inst, fd_p_cyc, fd_p_ref, fd_p_cache_ref, fd_p_cache_miss,
                    fd_p_br_instr, fd_p_br_miss, fd_p_aperf, fd_p_mperf, fd_p_c6, &stats->pcore);

    read_arch_stats(fd_e_inst, fd_e_cyc, fd_e_ref, fd_e_cache_ref, fd_e_cache_miss,
                    fd_e_br_instr, fd_e_br_miss, fd_e_aperf, fd_e_mperf, -1, &stats->ecore);

    /* 读取热裕度 */
    if (fd_thermal >= 0)
    {
        long long val = 0;
        if (read(fd_thermal, &val, sizeof(val)) != sizeof(val)) val = 0;
        stats->pcore.thermal_margin = (unsigned long long)val;
    }
    else
    {
        stats->pcore.thermal_margin = 0;
    }

    /* 读取 Package C10 */
    if (fd_pkg_c10 >= 0)
    {
        long long val = 0;
        if (read(fd_pkg_c10, &val, sizeof(val)) != sizeof(val)) val = 0;
        stats->pkg_c10_residency = (unsigned long long)val;
    }

    return 0;
}

int cpu_monitor_compute(const cpu_stats_t *prev, const cpu_stats_t *cur, cpu_metrics_t *out)
{
    if (!prev || !cur || !out)
    {
        return -1;
    }

    /* P-Core 增量 */
    long long p_delta_inst = (long long)(cur->pcore.instructions - prev->pcore.instructions);
    long long p_delta_cyc = (long long)(cur->pcore.cycles - prev->pcore.cycles);
    long long p_delta_aperf = (long long)(cur->pcore.aperf - prev->pcore.aperf);
    long long p_delta_mperf = (long long)(cur->pcore.mperf - prev->pcore.mperf);
    long long p_delta_c6 = (long long)(cur->pcore.c6_residency - prev->pcore.c6_residency);

    /* E-Core 增量 */
    long long e_delta_inst = (long long)(cur->ecore.instructions - prev->ecore.instructions);
    long long e_delta_cyc = (long long)(cur->ecore.cycles - prev->ecore.cycles);
    long long e_delta_aperf = (long long)(cur->ecore.aperf - prev->ecore.aperf);
    long long e_delta_mperf = (long long)(cur->ecore.mperf - prev->ecore.mperf);

    /* Package C10 增量 */
    long long delta_c10 = (long long)(cur->pkg_c10_residency - prev->pkg_c10_residency);

    /* --- P-Core 频率 --- */
    if (p_delta_mperf > 0 && prev->pcore.mperf > 0)
    {
        out->pcore_freq_mhz = ((double)p_delta_aperf / (double)p_delta_mperf) * tsc_freq_mhz;
    }
    else
    {
        out->pcore_freq_mhz = 0.0;
    }

    /* --- E-Core 频率 --- */
    if (e_delta_mperf > 0 && prev->ecore.mperf > 0)
    {
        out->ecore_freq_mhz = ((double)e_delta_aperf / (double)e_delta_mperf) * tsc_freq_mhz;
    }
    else
    {
        out->ecore_freq_mhz = 0.0;
    }

    /* --- P-Core IPC --- */
    if (p_delta_cyc > 0 && prev->pcore.cycles > 0)
    {
        out->pcore_ipc = (double)p_delta_inst / (double)p_delta_cyc;
    }
    else
    {
        out->pcore_ipc = 0.0;
    }

    /* --- E-Core IPC --- */
    if (e_delta_cyc > 0 && prev->ecore.cycles > 0)
    {
        out->ecore_ipc = (double)e_delta_inst / (double)e_delta_cyc;
    }
    else
    {
        out->ecore_ipc = 0.0;
    }

    /* --- P-Core 缓存未命中率 --- */
    long long p_delta_ref = (long long)(cur->pcore.cache_refs - prev->pcore.cache_refs);
    long long p_delta_miss = (long long)(cur->pcore.cache_misses - prev->pcore.cache_misses);
    if (p_delta_ref > 0 && prev->pcore.cache_refs > 0)
    {
        out->pcore_cache_miss_rate = (double)p_delta_miss / (double)p_delta_ref * 100.0;
    }
    else
    {
        out->pcore_cache_miss_rate = 0.0;
    }

    /* --- E-Core 缓存未命中率 --- */
    long long e_delta_ref = (long long)(cur->ecore.cache_refs - prev->ecore.cache_refs);
    long long e_delta_miss = (long long)(cur->ecore.cache_misses - prev->ecore.cache_misses);
    if (e_delta_ref > 0 && prev->ecore.cache_refs > 0)
    {
        out->ecore_cache_miss_rate = (double)e_delta_miss / (double)e_delta_ref * 100.0;
    }
    else
    {
        out->ecore_cache_miss_rate = 0.0;
    }

    /* --- P-Core 分支未命中率 --- */
    long long p_delta_br_instr = (long long)(cur->pcore.branch_instructions - prev->pcore.branch_instructions);
    long long p_delta_br = (long long)(cur->pcore.branch_misses - prev->pcore.branch_misses);
    if (p_delta_br_instr > 0)
    {
        out->pcore_branch_miss_rate = (double)p_delta_br / (double)p_delta_br_instr * 100.0;
    }
    else
    {
        out->pcore_branch_miss_rate = 0.0;
    }

    /* --- E-Core 分支未命中率 --- */
    long long e_delta_br_instr = (long long)(cur->ecore.branch_instructions - prev->ecore.branch_instructions);
    long long e_delta_br = (long long)(cur->ecore.branch_misses - prev->ecore.branch_misses);
    if (e_delta_br_instr > 0)
    {
        out->ecore_branch_miss_rate = (double)e_delta_br / (double)e_delta_br_instr * 100.0;
    }
    else
    {
        out->ecore_branch_miss_rate = 0.0;
    }

    /* --- Core C6 驻留 --- */
    if (p_delta_mperf > 0 && prev->pcore.mperf > 0)
    {
        out->core_c6_pct = (double)p_delta_c6 / (double)p_delta_mperf * 100.0 / 4.0;
    }
    else
    {
        out->core_c6_pct = 0.0;
    }
    if (out->core_c6_pct > 100.0)
    {
        out->core_c6_pct = 100.0;
    }

    /* --- Package C10 驻留 --- */
    if (p_delta_mperf > 0 && prev->pcore.mperf > 0)
    {
        out->pkg_c10_pct = (double)delta_c10 / (double)p_delta_mperf * 100.0;
    }
    else
    {
        out->pkg_c10_pct = 0.0;
    }

    /* --- 热裕度 --- */
    out->thermal_margin_degrees = (int)cur->pcore.thermal_margin;

    return 0;
}

void cpu_monitor_cleanup(void)
{
    if (fd_p_inst != -1) close(fd_p_inst);
    if (fd_p_cyc != -1) close(fd_p_cyc);
    if (fd_p_ref != -1) close(fd_p_ref);
    if (fd_p_cache_ref != -1) close(fd_p_cache_ref);
    if (fd_p_cache_miss != -1) close(fd_p_cache_miss);
    if (fd_p_br_instr != -1) close(fd_p_br_instr);
    if (fd_p_br_miss != -1) close(fd_p_br_miss);
    if (fd_p_aperf != -1) close(fd_p_aperf);
    if (fd_p_mperf != -1) close(fd_p_mperf);
    if (fd_p_c6 != -1) close(fd_p_c6);

    if (fd_e_inst != -1) close(fd_e_inst);
    if (fd_e_cyc != -1) close(fd_e_cyc);
    if (fd_e_ref != -1) close(fd_e_ref);
    if (fd_e_cache_ref != -1) close(fd_e_cache_ref);
    if (fd_e_cache_miss != -1) close(fd_e_cache_miss);
    if (fd_e_br_instr != -1) close(fd_e_br_instr);
    if (fd_e_br_miss != -1) close(fd_e_br_miss);
    if (fd_e_aperf != -1) close(fd_e_aperf);
    if (fd_e_mperf != -1) close(fd_e_mperf);

    if (fd_pkg_c10 != -1) close(fd_pkg_c10);
    if (fd_thermal != -1) close(fd_thermal);

    fd_p_inst = fd_p_cyc = fd_p_ref = -1;
    fd_p_cache_ref = fd_p_cache_miss = -1;
    fd_p_br_instr = fd_p_br_miss = -1;
    fd_p_aperf = fd_p_mperf = fd_p_c6 = -1;

    fd_e_inst = fd_e_cyc = fd_e_ref = -1;
    fd_e_cache_ref = fd_e_cache_miss = -1;
    fd_e_br_instr = fd_e_br_miss = -1;
    fd_e_aperf = fd_e_mperf = -1;

    fd_pkg_c10 = -1;
    fd_thermal = -1;
}
