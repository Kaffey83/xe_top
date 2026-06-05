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

#include "config.h"
#include "../util/version.h"
#include "../monitor/paths.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>

void config_parse(int argc, char *argv[], runtime_config_t *cfg)
{
    cfg->interval_sec = 1;
    cfg->enable_gpu = true;
    cfg->enable_cpu = true;
    cfg->enable_power = true;
    cfg->enable_mem = true;
    cfg->enable_battery = true;
    cfg->enable_disk = true;
    cfg->enable_net = true;
    cfg->enable_npu = true;
    cfg->help_requested = false;

    static struct option long_opts[] = {
        {"interval", required_argument, NULL, 'i'},
        {"no-gpu", no_argument, NULL, 'G'},
        {"no-cpu", no_argument, NULL, 'C'},
        {"no-power", no_argument, NULL, 'P'},
        {"no-memory", no_argument, NULL, 'M'},
        {"no-battery", no_argument, NULL, 'B'},
        {"no-disk", no_argument, NULL, 'D'},
        {"no-net", no_argument, NULL, 'N'},
        {"no-npu", no_argument, NULL, 'A'},
        {"version", no_argument, NULL, 'V'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "i:GCPhMBDNAV", long_opts, NULL)) != -1)
    {
        switch (opt)
        {
            case 'i':
            {
                char *end = NULL;
                errno = 0;
                double v = strtod(optarg, &end);
                if (errno || end == optarg || *end != '\0' || v < INTERVAL_MIN_SEC || v > INTERVAL_MAX_SEC)
                {
                    fprintf(stderr, "Invalid interval: '%s' (valid range: %.1f - %.0f sec)\n",
                            optarg, INTERVAL_MIN_SEC, INTERVAL_MAX_SEC);
                    cfg->help_requested = true;
                    return;
                }
                cfg->interval_sec = v;
                break;
            }
            case 'G': cfg->enable_gpu = false; break;
            case 'C': cfg->enable_cpu = false; break;
            case 'P': cfg->enable_power = false; break;
            case 'M': cfg->enable_mem = false; break;
            case 'B': cfg->enable_battery = false; break;
            case 'D': cfg->enable_disk = false; break;
            case 'N': cfg->enable_net = false; break;
            case 'A': cfg->enable_npu = false; break;
            case 'V':
                printf("xe_top %s\n", XE_TOP_VERSION_STRING);
                cfg->help_requested = true;
                return;
            case 'h':
                cfg->help_requested = true;
                config_print_usage(argv[0] ? argv[0] : "xe_top");
                return;
            default:
                cfg->help_requested = true;
                config_print_usage(argv[0] ? argv[0] : "xe_top");
                return;
        }
    }
}

void config_print_usage(const char *prog_name)
{
    printf("Usage: %s [OPTIONS]\n\n", prog_name);
    printf("Options:\n");
    printf("  -i, --interval <sec>  Refresh interval (default: 1, range: %.1f-%.0f)\n", INTERVAL_MIN_SEC, INTERVAL_MAX_SEC);
    printf("  -G, --no-gpu         Disable GPU monitoring\n");
    printf("  -C, --no-cpu         Disable CPU monitoring\n");
    printf("  -P, --no-power       Disable power monitoring\n");
    printf("  -M, --no-memory      Disable memory monitoring (bandwidth + capacity)\n");
    printf("  -B, --no-battery     Disable battery monitoring\n");
    printf("  -D, --no-disk        Disable disk monitoring\n");
    printf("  -N, --no-net         Disable network monitoring\n");
    printf("  -A, --no-npu         Disable NPU monitoring\n");
    printf("  -V, --version        Show version number\n");
    printf("  -h, --help           Show this help message\n");
    printf("\nInteractive Keys:\n");
    printf("  q / Esc    Quit\n");
    printf("  + / -      Adjust refresh interval\n");
    printf("  p          Pause/resume\n");
    printf("  h          Show help\n");
}