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

#ifndef DISPLAY_H
#define DISPLAY_H

#include "../monitor/gpu_monitor.h"
#include "../monitor/cpu_monitor.h"
#include "../monitor/power_monitor.h"
#include "../monitor/mem_monitor.h"
#include "../monitor/battery_monitor.h"
#include "../monitor/disk_monitor.h"
#include "../monitor/net_monitor.h"
#include "../monitor/npu_monitor.h"

void display_init(void);
void display_render(const gpu_metrics_t *gpu, const cpu_metrics_t *cpu, const mem_stats_t *mem, const mem_capacity_t *mem_cap, const power_metrics_t *power, const battery_metrics_t *battery, const disk_metrics_t *disk, const net_metrics_t *net, const npu_metrics_t *npu);
void display_toggle_help(void);
void display_cleanup(void);

#endif /* DISPLAY_H */