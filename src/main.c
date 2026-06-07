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

#define _POSIX_C_SOURCE 199309L

#include "config/config.h"
#include "monitor/gpu_monitor.h"
#include "monitor/cpu_monitor.h"
#include "monitor/power_monitor.h"
#include "monitor/mem_monitor.h"
#include "monitor/battery_monitor.h"
#include "monitor/disk_monitor.h"
#include "monitor/net_monitor.h"
#include "monitor/npu_monitor.h"
#include "display/display.h"
#include "util/version.h"
#include "monitor/paths.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

static volatile sig_atomic_t running = 1;
static volatile sig_atomic_t paused = 0;

/* Handle exit signals uniformly to ensure terminal state is restored */
static void handle_signal(int sig)
{
    (void)sig;
    running = 0;
}

static void setup_signals(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
}

/* Process a single keypress; returns 1 if should quit, 0 otherwise */
static int process_key(char key, runtime_config_t *cfg)
{
    switch (key)
    {
        case 'q':
        case 'Q':
            return 1;
        /* NOTE: ESC (0x1B) is intentionally NOT handled here.
         * In raw mode it is the leading byte of every ANSI escape
         * sequence (mouse wheel, arrow keys, F1-F4, ...), and treating
         * a bare ESC as "quit" makes the program exit on any such
         * event. ESC sequences are consumed by the main loop instead. */
        case '+':
        case '=':
            if (cfg->interval_sec > INTERVAL_MIN_SEC)
            {
                cfg->interval_sec -= 0.1;
                if (cfg->interval_sec < INTERVAL_MIN_SEC)
                    cfg->interval_sec = INTERVAL_MIN_SEC;
            }
            break;
        case '-':
        case '_':
            if (cfg->interval_sec < INTERVAL_MAX_SEC)
            {
                cfg->interval_sec += 0.1;
                if (cfg->interval_sec > INTERVAL_MAX_SEC)
                    cfg->interval_sec = INTERVAL_MAX_SEC;
            }
            break;
        case 'p':
        case 'P':
            paused = !paused;
            break;
        case 'h':
        case 'H':
            display_toggle_help();
            break;
        default:
            break;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    /* --- Check if stdout is a terminal --- */
    if (!isatty(STDOUT_FILENO))
    {
        fprintf(stderr, "Error: xe_top must be run in a terminal (stdout is not a tty)\n");
        return 1;
    }

    /* --- Redirect stderr to /dev/null to avoid TUI corruption --- */
    FILE *devnull = freopen("/dev/null", "w", stderr);
    (void)devnull;

    /* --- Parse configuration --- */
    runtime_config_t cfg;
    config_parse(argc, argv, &cfg);

    if (cfg.help_requested)
    {
        return 0;
    }

    /* --- Initialize requested monitoring modules --- */
    int gpu_ok = cfg.enable_gpu ? (gpu_monitor_init() == 0) : 0;
    int cpu_ok = cfg.enable_cpu ? (cpu_monitor_init() == 0) : 0;
    int pwr_ok = cfg.enable_power ? (power_monitor_init() == 0) : 0;
    int mem_ok = cfg.enable_mem ? (mem_monitor_init() == 0) : 0;
    int bat_ok = cfg.enable_battery ? (battery_monitor_init() == 0) : 0;
    int dsk_ok = cfg.enable_disk ? (disk_monitor_init() == 0) : 0;
    int net_ok = cfg.enable_net ? (net_monitor_init() == 0) : 0;
    int npu_ok = cfg.enable_npu ? (npu_monitor_init() == 0) : 0;

    if (!gpu_ok && !cpu_ok && !pwr_ok && !mem_ok && !bat_ok && !dsk_ok && !net_ok && !npu_ok)
    {
        /* Restore stderr for the error message.
         * Note: under GCC >= 14, a plain (void) cast no longer
         * satisfies warn_unused_result; we must actually consume
         * the return value. */
        if (freopen("/dev/tty", "w", stderr) == NULL) {
            /* Nothing useful to do; just fall through. */
        }
        fprintf(stderr, "All monitor initialization failed, exiting.\n");
        goto cleanup;
    }

    /* --- Initialize fullscreen display and signal handling --- */
    display_init();
    setup_signals();

    /* --- Initial sampling --- */
    gpu_stats_t prev_gpu = {0};
    cpu_stats_t prev_cpu = {0};
    power_stats_t prev_pwr = {0};
    disk_stats_t prev_dsk = {0};
    net_stats_t prev_net = {0};
    npu_stats_t prev_npu = {0};

    if (gpu_ok) gpu_monitor_read(&prev_gpu);
    if (cpu_ok) cpu_monitor_read(&prev_cpu);
    if (pwr_ok) power_monitor_read(&prev_pwr);
    if (dsk_ok) disk_monitor_read(&prev_dsk);
    if (net_ok) net_monitor_read(&prev_net);
    if (npu_ok) npu_monitor_read(&prev_npu);

    struct timespec ts_start, ts_end;
    clock_gettime(CLOCK_MONOTONIC, &ts_start);

    /* --- Main loop --- */
    while (running)
    {
        /* Non-blocking read and process keypresses.
         *
         * The terminal is in raw mode (ICANON off, VMIN=0, VTIME=0),
         * so read() returns every byte immediately. Many "non-key"
         * events such as a mouse-wheel scroll, arrow keys, F1-F4,
         * Shift+Tab, etc. arrive as ANSI escape sequences whose first
         * byte is ESC (0x1B). We must NOT treat a bare ESC as the
         * "quit" key, or the program will exit on any such event.
         *
         * We implement a tiny state machine that, once it has seen an
         * ESC, swallows the rest of the sequence (parameter bytes,
         * intermediate bytes, and the final byte) before normal key
         * handling resumes. A second ESC starts a new sequence. */
        char key_buf[64];
        ssize_t n_read;
        int esc_pending = 0; /* 0 = normal, 1 = inside an ESC sequence */
        do {
            n_read = read(STDIN_FILENO, key_buf, sizeof(key_buf));
            if (n_read > 0)
            {
                for (ssize_t i = 0; i < n_read; i++)
                {
                    unsigned char c = (unsigned char)key_buf[i];

                    if (esc_pending)
                    {
                        /* Inside an escape sequence: keep consuming
                         * parameter / intermediate bytes. */
                        if (c == 0x1B) {
                            /* A second ESC starts a brand new
                             * sequence; the previous one is dropped. */
                            esc_pending = 1;
                            continue;
                        }
                        if (c == ';' || c == ':' || c == ',' ||
                            (c >= '0' && c <= '9') ||
                            c == '<' || c == '>' || c == '?' ||
                            (c >= ' ' && c <= '/')) {
                            /* CSI/SS3 parameter and intermediate bytes
                             * (0x20-0x2F). Keep consuming. */
                            continue;
                        }
                        /* Any other byte terminates the sequence.
                         * Drop it (do NOT forward to process_key) and
                         * return to normal mode. */
                        esc_pending = 0;
                        continue;
                    }

                    if (c == 0x1B) {
                        /* Start of a possible escape sequence.
                         * Defer the decision: the next byte tells us
                         * whether it is a real sequence to swallow or
                         * just a stray ESC we previously decided to
                         * ignore. process_key() is never called with
                         * 0x1B, so an ESC can no longer quit. */
                        esc_pending = 1;
                        continue;
                    }

                    if (process_key((char)c, &cfg))
                    {
                        running = 0;
                        goto loop_exit;
                    }
                }
            }
        } while (n_read > 0 || (n_read < 0 && errno == EINTR));

        /* Sub-second sleep */
        double interval = (double)cfg.interval_sec;
        struct timespec req = {
            .tv_sec = (time_t)interval,
            .tv_nsec = (long)((interval - (time_t)interval) * 1e9)
        };
        nanosleep(&req, NULL);

        /* Skip sampling if paused */
        if (paused)
        {
            continue;
        }

        /* Read current sample values */
        gpu_stats_t cur_gpu = {0};
        cpu_stats_t cur_cpu = {0};
        power_stats_t cur_pwr = {0};
        mem_stats_t cur_mem = {0};
        mem_capacity_t mem_cap = {0};
        int mem_cap_ok = (mem_monitor_read_capacity(&mem_cap) == 0);
        battery_metrics_t cur_bat = {0};
        disk_stats_t cur_dsk = {0};
        net_stats_t cur_net = {0};
        npu_stats_t cur_npu = {0};

        if (gpu_ok) gpu_monitor_read(&cur_gpu);
        if (cpu_ok) cpu_monitor_read(&cur_cpu);
        if (pwr_ok) power_monitor_read(&cur_pwr);
        if (bat_ok) battery_monitor_read(&cur_bat);
        if (dsk_ok) disk_monitor_read(&cur_dsk);
        if (net_ok) net_monitor_read(&cur_net);
        if (npu_ok) npu_monitor_read(&cur_npu);

        clock_gettime(CLOCK_MONOTONIC, &ts_end);
        double elapsed = (ts_end.tv_sec - ts_start.tv_sec) +
                         (double)(ts_end.tv_nsec - ts_start.tv_nsec) / 1e9;

        /* --- Compute metrics --- */
        gpu_metrics_t gpu_metrics = {0};
        cpu_metrics_t cpu_metrics = {0};
        power_metrics_t power_metrics = {0};
        disk_metrics_t disk_metrics = {0};
        net_metrics_t net_metrics = {0};
        npu_metrics_t npu_metrics = {0};

        if (gpu_ok && elapsed > 0.0) gpu_monitor_compute(&prev_gpu, &cur_gpu, elapsed, &gpu_metrics);
        if (cpu_ok) cpu_monitor_compute(&prev_cpu, &cur_cpu, &cpu_metrics);
        if (pwr_ok && elapsed > 0.0) power_monitor_compute(&prev_pwr, &cur_pwr, elapsed, &power_metrics);
        if (mem_ok && elapsed > 0.0) mem_monitor_calc(elapsed, &cur_mem);
        if (dsk_ok && elapsed > 0.0) disk_monitor_compute(&prev_dsk, &cur_dsk, elapsed, &disk_metrics);
        if (net_ok && elapsed > 0.0) net_monitor_compute(&prev_net, &cur_net, elapsed, &net_metrics);
        if (npu_ok && elapsed > 0.0) npu_monitor_compute(&prev_npu, &cur_npu, elapsed, &npu_metrics);

        /* --- Render fullscreen UI --- */
        display_render(gpu_ok ? &gpu_metrics : NULL,
                       cpu_ok ? &cpu_metrics : NULL,
                       mem_ok ? &cur_mem : NULL,
                       mem_cap_ok ? &mem_cap : NULL,
                       pwr_ok ? &power_metrics : NULL,
                       bat_ok ? &cur_bat : NULL,
                       dsk_ok ? &disk_metrics : NULL,
                       net_ok ? &net_metrics : NULL,
                       npu_ok ? &npu_metrics : NULL);

        /* --- Save state for next iteration --- */
        if (gpu_ok) prev_gpu = cur_gpu;
        if (cpu_ok) prev_cpu = cur_cpu;
        if (pwr_ok) prev_pwr = cur_pwr;
        if (dsk_ok) prev_dsk = cur_dsk;
        if (net_ok) prev_net = cur_net;
        if (npu_ok) prev_npu = cur_npu;

        ts_start = ts_end;
    }

loop_exit:
    /* --- Cleanup --- */
    display_cleanup();
    if (gpu_ok) gpu_monitor_cleanup();
    if (cpu_ok) cpu_monitor_cleanup();
    if (pwr_ok) power_monitor_cleanup();
    if (mem_ok) mem_monitor_cleanup();
    if (bat_ok) battery_monitor_cleanup();
    if (dsk_ok) disk_monitor_cleanup();
    if (net_ok) net_monitor_cleanup();
    if (npu_ok) npu_monitor_cleanup();

    return 0;

cleanup:
    if (gpu_ok) gpu_monitor_cleanup();
    if (cpu_ok) cpu_monitor_cleanup();
    if (pwr_ok) power_monitor_cleanup();
    if (mem_ok) mem_monitor_cleanup();
    if (bat_ok) battery_monitor_cleanup();
    if (dsk_ok) disk_monitor_cleanup();
    if (net_ok) net_monitor_cleanup();
    if (npu_ok) npu_monitor_cleanup();

    return 1;
}