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

#include "display.h"
#include "style.h"
#include "../util/ring.h"
#include "../util/version.h"
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>

/* Terminal state */
static int term_cols = 80;
static int term_rows = 24;
static int prev_term_cols = -1;
static int prev_term_rows = -1;
static struct termios orig_termios;
static int show_help = 0;

/* Sparkline ring buffers (60 samples each) */
#define SPARK_LEN 60
static double spark_gpu_util_buf[SPARK_LEN];
static double spark_cpu_pcore_buf[SPARK_LEN];
static double spark_cpu_ecore_buf[SPARK_LEN];
static double spark_npu_util_buf[SPARK_LEN];
static double spark_mem_usage_buf[SPARK_LEN];
static double spark_pkg_power_buf[SPARK_LEN];

static ring_buf_t spark_gpu_util;
static ring_buf_t spark_cpu_pcore;
static ring_buf_t spark_cpu_ecore;
static ring_buf_t spark_npu_util;
static ring_buf_t spark_mem_usage;
static ring_buf_t spark_pkg_power;

/* Layout computation */
typedef struct {
    int label_x;    /* Label start column */
    int bar_x;      /* Progress bar start column */
    int value_x;    /* Value start column */
    int bar_w;      /* Progress bar width */
    int spark_w;    /* Sparkline width */
} row_layout_t;

static void layout_compute(int term_w, row_layout_t *l)
{
    l->label_x = 3;
    l->bar_w = term_w - 55;
    if (l->bar_w > 40) l->bar_w = 40;
    if (l->bar_w < 10) l->bar_w = 10;
    l->bar_x = 35;
    l->value_x = l->bar_x + l->bar_w + 2;
    l->spark_w = term_w - 4;
    if (l->spark_w > SPARK_LEN) l->spark_w = SPARK_LEN;
    if (l->spark_w < 10) l->spark_w = 10;
}

static void gotoxy(int row, int col)
{
    printf("\033[%d;%dH", row, col);
}

static void phoriz(int n)
{
    for (int i = 0; i < n; i++)
    {
        fputs(HORIZ, stdout);
    }
}

/* Sparkline rendering */
static void draw_sparkline(const ring_buf_t *r, int width)
{
    if (width < 2 || r->count < 2) return;

    double mn = ring_min(r);
    double mx = ring_max(r);
    double range = mx - mn;
    if (range < 1e-9) range = 1.0;

    int count = r->count;
    if (count > width) count = width;
    int start_idx = r->count - count;

    fputs(COLOR_DIM, stdout);
    for (int i = 0; i < count; i++)
    {
        double val = ring_get(r, start_idx + i);
        int level = (int)((val - mn) / range * 7.0);
        if (level < 0) level = 0;
        if (level > 7) level = 7;
        const char *chars = SPARK_CHARS;
        putchar(chars[level * 3]);
        putchar(chars[level * 3 + 1]);
        putchar(chars[level * 3 + 2]);
    }
    fputs(COLOR_RESET, stdout);
}

/* Dynamic progress bar (btop style, adaptive width) */
static void draw_bar(float pct, int width)
{
    if (width < 4) return;
    int filled = (int)((pct / 100.0f) * (width - 2));
    if (filled < 0) filled = 0;
    if (filled > width - 2) filled = width - 2;

    fputs(COLOR_RESET COLOR_DIM "[", stdout);
    fputs(color_for_pct(pct), stdout);
    for (int i = 0; i < filled; i++) fputs("■", stdout);
    fputs(COLOR_RESET COLOR_DIM, stdout);
    for (int i = filled; i < width - 2; i++) fputs("░", stdout);
    fputs("]" COLOR_RESET, stdout);
}

/* Draw full-width panel frame */
static void draw_panel(int row, int h, int w, const char *title)
{
    if (h < 2 || w < 4) return;

    gotoxy(row, 1);
    fputs(COLOR_DIM, stdout);
    fputs(TL, stdout);
    fputs(HORIZ, stdout);
    fputs(COLOR_RESET, stdout);
    fputc(' ', stdout);
    if (title)
    {
        fputs(COLOR_BOLD COLOR_CYAN, stdout);
        fputs(title, stdout);
        fputs(COLOR_RESET, stdout);
        fputc(' ', stdout);
    }
    int remain = w - (title ? (int)strlen(title) : 0) - 5;
    if (remain < 0) remain = 0;
    fputs(COLOR_DIM, stdout);
    phoriz(remain);
    fputs(TR, stdout);
    fputs(COLOR_RESET, stdout);

    for (int r = 1; r < h - 1; r++)
    {
        gotoxy(row + r, 1);
        fputs(COLOR_DIM VERT COLOR_RESET, stdout);
        gotoxy(row + r, w);
        fputs(COLOR_DIM VERT COLOR_RESET, stdout);
    }

    gotoxy(row + h - 1, 1);
    fputs(COLOR_DIM, stdout);
    fputs(BL, stdout);
    phoriz(w - 2);
    fputs(BR, stdout);
    fputs(COLOR_RESET, stdout);
}

void display_init(void)
{
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0)
    {
        term_cols = ws.ws_col;
        term_rows = ws.ws_row;
    }

    ring_init(&spark_gpu_util, spark_gpu_util_buf, SPARK_LEN);
    ring_init(&spark_cpu_pcore, spark_cpu_pcore_buf, SPARK_LEN);
    ring_init(&spark_cpu_ecore, spark_cpu_ecore_buf, SPARK_LEN);
    ring_init(&spark_npu_util, spark_npu_util_buf, SPARK_LEN);
    ring_init(&spark_mem_usage, spark_mem_usage_buf, SPARK_LEN);
    ring_init(&spark_pkg_power, spark_pkg_power_buf, SPARK_LEN);

    printf("\033[?1049h");
    fflush(stdout);

    tcgetattr(STDIN_FILENO, &orig_termios);
    struct termios new_termios = orig_termios;
    new_termios.c_lflag &= ~(ECHO | ICANON);
    new_termios.c_cc[VMIN] = 0;
    new_termios.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);
    tcflush(STDIN_FILENO, TCIFLUSH);

    printf("\033[?25l\033[2J");
    fflush(stdout);
}

void display_render(const gpu_metrics_t *gpu, const cpu_metrics_t *cpu, const mem_stats_t *mem, const mem_capacity_t *mem_cap, const power_metrics_t *power, const battery_metrics_t *battery, const disk_metrics_t *disk, const net_metrics_t *net, const npu_metrics_t *npu)
{
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0)
    {
        term_cols = ws.ws_col;
        term_rows = ws.ws_row;
    }

    if (term_cols != prev_term_cols || term_rows != prev_term_rows)
    {
        fputs("\033[2J", stdout);
        prev_term_cols = term_cols;
        prev_term_rows = term_rows;
    }

    int w = term_cols;
    int cur_row = 1;
    char buf[256];
    row_layout_t layout;
    layout_compute(w, &layout);

    /* Push sparkline samples */
    if (gpu) ring_push(&spark_gpu_util, gpu->utilization);
    if (cpu) {
        ring_push(&spark_cpu_pcore, cpu->pcore_freq_mhz);
        ring_push(&spark_cpu_ecore, cpu->ecore_freq_mhz);
    }
    if (npu) ring_push(&spark_npu_util, npu->utilization_pct);
    if (mem_cap) ring_push(&spark_mem_usage, mem_cap->usage_pct);
    if (power) ring_push(&spark_pkg_power, power->pkg_watts);

    /* Title bar */
    gotoxy(cur_row, 1);
    fputs("\033[K", stdout);
    const char *title_str = " INTEL CORE ULTRA SOC MONITOR ";
    int x = (w - (int)strlen(title_str)) / 2;
    if (x < 1) x = 1;
    gotoxy(cur_row, x);
    fputs(COLOR_BOLD COLOR_MAGENTA, stdout);
    fputs(title_str, stdout);
    fputs(COLOR_RESET, stdout);
    cur_row += 2;

    /* CPU panel */
    int cpu_h = 10;
    draw_panel(cur_row, cpu_h, w, "CPU (P-Core / E-Core)");
    if (cpu)
    {
        gotoxy(cur_row + 1, 3);
        fputs(COLOR_YELLOW, stdout);
        snprintf(buf, sizeof(buf), "P-Core: %5.1f MHz  IPC: %.2f", cpu->pcore_freq_mhz, cpu->pcore_ipc);
        fputs(buf, stdout);
        fputs(COLOR_RESET, stdout);
        gotoxy(cur_row + 1, layout.bar_x);
        draw_bar((float)cpu->pcore_cache_miss_rate, layout.bar_w);
        snprintf(buf, sizeof(buf), " L3: %5.2f%%", cpu->pcore_cache_miss_rate);
        fputs(buf, stdout);
        fputs(COLOR_RESET "\033[K", stdout);

        gotoxy(cur_row + 2, 3);
        snprintf(buf, sizeof(buf), "        Br Miss: %5.2f%%", cpu->pcore_branch_miss_rate);
        fputs(buf, stdout);
        fputs(COLOR_RESET "\033[K", stdout);

        gotoxy(cur_row + 3, 3);
        fputs(COLOR_DIM, stdout);
        draw_sparkline(&spark_cpu_pcore, layout.spark_w);
        fputs(COLOR_RESET "\033[K", stdout);

        gotoxy(cur_row + 4, 3);
        fputs(COLOR_BLUE, stdout);
        snprintf(buf, sizeof(buf), "E-Core: %5.1f MHz  IPC: %.2f", cpu->ecore_freq_mhz, cpu->ecore_ipc);
        fputs(buf, stdout);
        fputs(COLOR_RESET, stdout);
        gotoxy(cur_row + 4, layout.bar_x);
        draw_bar((float)cpu->ecore_cache_miss_rate, layout.bar_w);
        snprintf(buf, sizeof(buf), " L3: %5.2f%%", cpu->ecore_cache_miss_rate);
        fputs(buf, stdout);
        fputs(COLOR_RESET "\033[K", stdout);

        gotoxy(cur_row + 5, 3);
        snprintf(buf, sizeof(buf), "        Br Miss: %5.2f%%", cpu->ecore_branch_miss_rate);
        fputs(buf, stdout);
        fputs(COLOR_RESET "\033[K", stdout);

        gotoxy(cur_row + 6, 3);
        fputs(COLOR_DIM, stdout);
        draw_sparkline(&spark_cpu_ecore, layout.spark_w);
        fputs(COLOR_RESET "\033[K", stdout);

        gotoxy(cur_row + 7, 3);
        snprintf(buf, sizeof(buf), "Core C6: %5.1f%% ", cpu->core_c6_pct);
        fputs(buf, stdout);
        draw_bar((float)cpu->core_c6_pct, 15);
        snprintf(buf, sizeof(buf), " Pkg C10: %5.1f%% ", cpu->pkg_c10_pct);
        fputs(buf, stdout);
        draw_bar((float)cpu->pkg_c10_pct, 15);
        fputs(COLOR_RESET "\033[K", stdout);

        gotoxy(cur_row + 8, 3);
        if (cpu->thermal_margin_degrees >= 0)
        {
            fputs(COLOR_BOLD, stdout);
            snprintf(buf, sizeof(buf), "Thermal Margin: %3d C", cpu->thermal_margin_degrees);
            fputs(buf, stdout);
            fputs(COLOR_RESET, stdout);
        }
        else
        {
            fputs(COLOR_DIM "Thermal Margin: N/A" COLOR_RESET, stdout);
        }
        fputs("\033[K", stdout);
    }
    else
    {
        gotoxy(cur_row + 4, 3);
        fputs(COLOR_DIM "(disabled)" COLOR_RESET "\033[K", stdout);
    }
    gotoxy(cur_row + cpu_h, 1); fputs("\033[K", stdout);
    cur_row += cpu_h + 1;

    /* GPU & NPU panel */
    int gpu_h = 8;
    draw_panel(cur_row, gpu_h, w, "Xe Graphics & NPU");
    if (gpu)
    {
        gotoxy(cur_row + 1, 3);
        snprintf(buf, sizeof(buf), "Render: %5.1f%% ", gpu->utilization);
        fputs(buf, stdout);
        draw_bar((float)gpu->utilization, 30);
        fputs(COLOR_RESET "\033[K", stdout);

        gotoxy(cur_row + 2, 3);
        if (gpu->video_available)
        {
            fputs(COLOR_GREEN, stdout);
            snprintf(buf, sizeof(buf), "Video Decode: %5.1f%% ", gpu->video_utilization);
            fputs(buf, stdout);
            fputs(COLOR_RESET, stdout);
            draw_bar((float)gpu->video_utilization, 30);
        }
        else
        {
            fputs(COLOR_DIM "Video Decode: (unsupported)" COLOR_RESET, stdout);
        }
        fputs(COLOR_RESET "\033[K", stdout);

        gotoxy(cur_row + 3, 3);
        fputs(COLOR_CYAN, stdout);
        snprintf(buf, sizeof(buf), "Freq: %5.1f MHz   VRAM: %6.2f MiB (shared)", gpu->frequency_mhz, gpu->vram_used_mib);
        fputs(buf, stdout);
        fputs(COLOR_RESET "\033[K", stdout);

        gotoxy(cur_row + 4, 3);
        fputs(COLOR_DIM, stdout);
        draw_sparkline(&spark_gpu_util, layout.spark_w);
        fputs(COLOR_RESET "\033[K", stdout);
    }
    else
    {
        gotoxy(cur_row + 2, 3);
        fputs(COLOR_DIM "(disabled)" COLOR_RESET "\033[K", stdout);
    }

    /* NPU section */
    if (npu)
    {
        gotoxy(cur_row + 5, 3);
        fputs(COLOR_MAGENTA, stdout);
        snprintf(buf, sizeof(buf), "AI Engine (NPU): %5.1f%% ", npu->utilization_pct);
        fputs(buf, stdout);
        fputs(COLOR_RESET, stdout);
        draw_bar((float)npu->utilization_pct, 20);

        if (npu->max_freq_mhz > 0.0)
        {
            snprintf(buf, sizeof(buf), " Freq: %4.0f / %4.0f MHz", npu->freq_mhz, npu->max_freq_mhz);
            fputs(buf, stdout);
        }
        else
        {
            fputs(COLOR_DIM " Freq: N/A (driver unsupported)" COLOR_RESET, stdout);
        }
        fputs(COLOR_RESET "\033[K", stdout);

        gotoxy(cur_row + 6, 3);
        fputs(COLOR_CYAN, stdout);
        snprintf(buf, sizeof(buf), "NPU Memory: %6.2f MiB", npu->mem_mib);
        fputs(buf, stdout);
        fputs(COLOR_RESET "\033[K", stdout);

        gotoxy(cur_row + 7, 3);
        fputs(COLOR_DIM, stdout);
        draw_sparkline(&spark_npu_util, layout.spark_w);
        fputs(COLOR_RESET "\033[K", stdout);
    }
    else
    {
        gotoxy(cur_row + 5, 3);
        fputs(COLOR_DIM "(NPU disabled)" COLOR_RESET "\033[K", stdout);
    }
    gotoxy(cur_row + gpu_h, 1); fputs("\033[K", stdout);
    cur_row += gpu_h + 1;

    /* SOC panel (Memory + Power + Battery) */
    int soc_h = 8;
    draw_panel(cur_row, soc_h, w, "SOC (Memory & Power & Battery)");
    if (mem_cap || mem || power || battery)
    {
        int row_off = 1;
        if (mem_cap)
        {
            gotoxy(cur_row + row_off, 3);
            fputs(COLOR_GREEN, stdout);
            double used_gib = mem_cap->used_mib / 1024.0;
            double total_gib = mem_cap->total_mib / 1024.0;
            snprintf(buf, sizeof(buf), "RAM: %5.2f / %5.2f GiB", used_gib, total_gib);
            fputs(buf, stdout);
            fputs(COLOR_RESET, stdout);
            fputs("  ", stdout);
            draw_bar((float)mem_cap->usage_pct, 15);
            fputs("  ", stdout);
            snprintf(buf, sizeof(buf), "%5.1f%%", mem_cap->usage_pct);
            fputs(buf, stdout);
            fputs(COLOR_RESET "\033[K", stdout);
            row_off++;
        }
        else
        {
            gotoxy(cur_row + row_off, 3);
            fputs(COLOR_DIM "(RAM disabled)" COLOR_RESET "\033[K", stdout);
            row_off++;
        }
        if (mem)
        {
            gotoxy(cur_row + row_off, 3);
            fputs(COLOR_GREEN, stdout);
            snprintf(buf, sizeof(buf), "MEM Read: %7.2f MiB/s", mem->read_mib_s);
            fputs(buf, stdout);
            fputs(COLOR_RESET, stdout);
            gotoxy(cur_row + row_off, 35);
            fputs(COLOR_BLUE, stdout);
            snprintf(buf, sizeof(buf), "Write: %7.2f MiB/s", mem->write_mib_s);
            fputs(buf, stdout);
            fputs(COLOR_RESET "\033[K", stdout);
            row_off++;
        }
        if (power)
        {
            gotoxy(cur_row + row_off, 3);
            fputs(COLOR_YELLOW, stdout);
            snprintf(buf, sizeof(buf), "PWR Pkg: %5.2f W", power->pkg_watts);
            fputs(buf, stdout);
            fputs(COLOR_RESET, stdout);
            gotoxy(cur_row + row_off, 25);
            snprintf(buf, sizeof(buf), "Core: %5.2f W", power->core_watts);
            fputs(buf, stdout);
            gotoxy(cur_row + row_off, 45);
            snprintf(buf, sizeof(buf), "DRAM: %5.2f W", power->dram_watts);
            fputs(buf, stdout);
            fputs(COLOR_RESET "\033[K", stdout);
            row_off++;

            gotoxy(cur_row + row_off, 3);
            fputs(COLOR_DIM, stdout);
            draw_sparkline(&spark_pkg_power, layout.spark_w);
            fputs(COLOR_RESET "\033[K", stdout);
            row_off++;
        }
        if (battery)
        {
            gotoxy(cur_row + row_off, 3);
            fputs(COLOR_CYAN, stdout);

            const char *status_text = "Unknown";
            if (battery->status == BATTERY_STATUS_CHARGING)
                status_text = "Charging";
            else if (battery->status == BATTERY_STATUS_DISCHARGING)
                status_text = "Discharging";
            else if (battery->status == BATTERY_STATUS_FULL)
                status_text = "Full";

            snprintf(buf, sizeof(buf), "BAT %s %3d%% %5.2fW", status_text, battery->capacity, battery->watts);
            fputs(buf, stdout);
            fputs(COLOR_RESET, stdout);

            gotoxy(cur_row + row_off, 50);
            draw_bar((float)battery->capacity, 20);
            fputs(COLOR_RESET "\033[K", stdout);
        }
    }
    else
    {
        gotoxy(cur_row + 2, 3);
        fputs(COLOR_DIM "(disabled)" COLOR_RESET "\033[K", stdout);
    }
    gotoxy(cur_row + soc_h, 1); fputs("\033[K", stdout);
    cur_row += soc_h + 1;

    /* IO panel (Disk & Network) */
    int io_h = 4;
    draw_panel(cur_row, io_h, w, "IO (Disk & Network)");
    if (disk || net)
    {
        int row_off = 1;
        if (disk)
        {
            gotoxy(cur_row + row_off, 3);
            fputs(COLOR_MAGENTA, stdout);
            snprintf(buf, sizeof(buf), "DSK Read: %7.2f MiB/s", disk->read_mib_s);
            fputs(buf, stdout);
            fputs(COLOR_RESET, stdout);
            gotoxy(cur_row + row_off, 35);
            fputs(COLOR_RED, stdout);
            snprintf(buf, sizeof(buf), "Write: %7.2f MiB/s", disk->write_mib_s);
            fputs(buf, stdout);
            fputs(COLOR_RESET "\033[K", stdout);
            row_off++;
        }
        if (net)
        {
            gotoxy(cur_row + row_off, 3);
            fputs(COLOR_CYAN, stdout);
            snprintf(buf, sizeof(buf), "NET Down: %7.2f MiB/s", net->rx_mib_s);
            fputs(buf, stdout);
            fputs(COLOR_RESET, stdout);
            gotoxy(cur_row + row_off, 35);
            fputs(COLOR_GREEN, stdout);
            snprintf(buf, sizeof(buf), "Up: %7.2f MiB/s", net->tx_mib_s);
            fputs(buf, stdout);
            fputs(COLOR_RESET "\033[K", stdout);
            row_off++;
        }
    }
    else
    {
        gotoxy(cur_row + 1, 3);
        fputs(COLOR_DIM "(disabled)" COLOR_RESET "\033[K", stdout);
    }
    gotoxy(cur_row + io_h, 1); fputs("\033[K", stdout);
    cur_row += io_h + 1;

    /* Clear remaining lines below content */
    for (int r = cur_row; r <= term_rows; r++)
    {
        gotoxy(r, 1);
        fputs("\033[K", stdout);
    }

    /* Help overlay */
    if (show_help)
    {
        int help_w = 48, help_h = 12;
        int help_row = (term_rows - help_h) / 2;
        int help_col = (w - help_w) / 2;
        if (help_row < 2) help_row = 2;
        if (help_col < 2) help_col = 2;

        /* Semi-transparent background effect via dim */
        for (int r = help_row; r < help_row + help_h; r++)
        {
            gotoxy(r, help_col);
            fputs(COLOR_DIM "\033[7m", stdout); /* reverse video */
            for (int c = 0; c < help_w; c++) fputc(' ', stdout);
            fputs(COLOR_RESET, stdout);
        }

        gotoxy(help_row, help_col + 2);
        fputs(COLOR_BOLD COLOR_CYAN " Help " COLOR_RESET, stdout);
        gotoxy(help_row + 2, help_col + 3);
        fputs(COLOR_YELLOW "  q / Esc    " COLOR_RESET "Quit xe_top", stdout);
        gotoxy(help_row + 3, help_col + 3);
        fputs(COLOR_YELLOW "  + / -      " COLOR_RESET "Adjust refresh interval", stdout);
        gotoxy(help_row + 4, help_col + 3);
        fputs(COLOR_YELLOW "  p          " COLOR_RESET "Pause / Resume", stdout);
        gotoxy(help_row + 5, help_col + 3);
        fputs(COLOR_YELLOW "  h          " COLOR_RESET "Toggle this help", stdout);
        gotoxy(help_row + 7, help_col + 3);
        fputs(COLOR_DIM "  Press h again to close" COLOR_RESET, stdout);
    }

    /* Bottom help bar */
    gotoxy(term_rows, 1);
    fputs(COLOR_DIM, stdout);
    fputs(" q:quit  +/-:interval  p:pause  h:help  ", stdout);
    fputs(COLOR_RESET, stdout);
    fputs("\033[K", stdout);
    fflush(stdout);
}

void display_toggle_help(void)
{
    show_help = !show_help;
}

void display_cleanup(void)
{
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
    printf("\033[?25h\033[2J\033[H\033[?1049l");
    fflush(stdout);
}