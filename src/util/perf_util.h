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


#ifndef PERF_UTIL_H
#define PERF_UTIL_H

#include <stdint.h>
#include <sys/types.h>
#include <linux/perf_event.h>

/*
 * Unified wrapper for perf_event_open syscall.
 */
long perf_util_open(struct perf_event_attr *hw_event, pid_t pid, int cpu, int group_fd, unsigned long flags);

/*
 * Read a PMU type ID from sysfs, e.g. /sys/bus/event_source/devices/<name>/type.
 * Returns the type ID on success, or -1 on failure.
 */
int perf_util_get_pmu_type(const char *pmu_name);

/*
 * Parse a format description from sysfs.
 * Format file content example: "config:0-11" or "config:20-27"
 * Returns a packed value: (offset << 8) | length.
 * If the file cannot be read, returns the provided defaults.
 */
int perf_util_parse_format(const char *pmu_path, const char *field,
                           int default_offset, int default_length);

/*
 * Parse an event code from sysfs, e.g. /sys/devices/<pmu>/events/<event_name>.
 * File content example: "event=0x1234"
 * Returns the hex event code on success, or 0 on failure.
 */
unsigned long long perf_util_parse_event_code(const char *pmu_path, const char *event_name);

/*
 * Build a perf_event config value from multiple fields at their respective bit positions.
 * The fmt_info array contains packed values from perf_util_parse_format,
 * and the vals array contains the corresponding values to place at those positions.
 */
unsigned long long perf_util_build_config(unsigned long long *fmt_info,
                                          unsigned long long *vals,
                                          int num_fields);

/*
 * Open a perf event with the given type and config, bound to a specific CPU.
 * If pmu_path is non-NULL, attempts to read cpumask from <pmu_path>/cpumask
 * to determine the correct CPU.
 * Returns the fd on success, or -1 on failure.
 */
int perf_util_open_event(int pmu_type, unsigned long long config, const char *pmu_path);

/*
 * Read the first CPU number from a PMU's cpumask or cpus sysfs file.
 * Tries <pmu_path>/cpumask first, then /sys/bus/event_source/devices/<name>/cpus.
 * Returns the CPU number on success, or -1 if neither file exists.
 */
int perf_util_get_first_cpu(const char *pmu_name);

/*
 * Read a 64-bit counter value from a perf event fd.
 * Returns 0 on success, -1 on failure.
 */
int perf_util_read_counter(int fd, unsigned long long *value);

#endif /* PERF_UTIL_H */