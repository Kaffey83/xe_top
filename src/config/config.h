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

#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>

/* Runtime configuration parsed from command line arguments */
typedef struct
{
    double interval_sec;       /* Refresh interval (default: 1) */
    bool enable_gpu;        /* Enable GPU monitoring (default: true) */
    bool enable_cpu;        /* Enable CPU monitoring (default: true) */
    bool enable_power;      /* Enable power monitoring (default: true) */
    bool enable_mem;        /* Enable memory monitoring (default: true) */
    bool enable_battery;    /* Enable battery monitoring (default: true) */
    bool enable_disk;       /* Enable disk monitoring (default: true) */
    bool enable_net;        /* Enable network monitoring (default: true) */
    bool enable_npu;        /* Enable NPU monitoring (default: true) */
    bool help_requested;    /* Help requested (--help) */
} runtime_config_t;

void config_parse(int argc, char *argv[], runtime_config_t *cfg);
void config_print_usage(const char *prog_name);

#endif /* CONFIG_H */