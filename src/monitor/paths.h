// SPDX-License-Identifier: GPL-2.0
/*
 * xe_top - Intel Core Ultra Full-Stack Performance Monitoring Tool
 *
 * Copyright (C) 2025
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef PATHS_H
#define PATHS_H

/* ── PMU sysfs paths ── */
#define PMU_BUS_PATH    "/sys/bus/event_source/devices"
#define CPU_CORE_PATH   "/sys/bus/event_source/devices/cpu_core"
#define CPU_ATOM_PATH   "/sys/bus/event_source/devices/cpu_atom"
#define MSR_PATH        "/sys/bus/event_source/devices/msr"
#define CSTATE_CORE_PATH "/sys/bus/event_source/devices/cstate_core"
#define CSTATE_PKG_PATH  "/sys/bus/event_source/devices/cstate_pkg"

/* ── RAPL power paths ── */
#define RAPL_PKG_PATH   "/sys/class/powercap/intel-rapl:0/energy_uj"
#define RAPL_CORE_PATH  "/sys/class/powercap/intel-rapl:0:0/energy_uj"
#define RAPL_DRAM_PATH  "/sys/class/powercap/intel-rapl:0:1/energy_uj"

/* ── NPU device path ── */
#define NPU_DEV_PATH    "/sys/class/accel/accel0/device"

/* ── System paths ── */
#define PROC_PATH       "/proc"
#define MEMINFO_PATH    "/proc/meminfo"
#define SYS_BLOCK_PATH  "/sys/block"
#define SYS_NET_PATH    "/sys/class/net"
#define POWER_SUPPLY_PATH "/sys/class/power_supply"
#define TSC_FREQ_PATH   "/sys/devices/system/cpu/cpu0/tsc_freq_khz"

/* ── Tunable constants ── */
#define GTT_SCAN_INTERVAL_DEFAULT  3
#define MAX_XE_CLIENTS             256
#define INTERVAL_MIN_SEC           0.1
#define INTERVAL_MAX_SEC           60.0

#endif /* PATHS_H */