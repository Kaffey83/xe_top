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
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>

/* ── 框线字符 (UTF-8) ── */
#define HORIZ "─"
#define VERT "│"
#define TL "┌"
#define TR "┐"
#define BL "└"
#define BR "┘"

/* ── ANSI 颜色代码 ── */
#define COLOR_RESET "\033[0m"
#define COLOR_BOLD "\033[1m"
#define COLOR_DIM "\033[2m"
#define COLOR_CYAN "\033[36m"
#define COLOR_GREEN "\033[32m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_RED "\033[31m"
#define COLOR_BLUE "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_BG_BLACK "\033[40m"

static int term_cols = 80;
static int term_rows = 24;
static int prev_term_cols = -1;
static int prev_term_rows = -1;
static struct termios orig_termios;

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

/* ── 动态进度条（btop 风格，宽度自适应）── */
static void draw_bar(float pct, int width)
{
    if (width < 4)
    {
        return;
    }
    int filled = (int)((pct / 100.0f) * (width - 2));
    if (filled < 0)
    {
        filled = 0;
    }
    if (filled > width - 2)
    {
        filled = width - 2;
    }

    /* 1. 左括号：强制重置前置颜色，设为暗灰色 */
    fputs(COLOR_RESET COLOR_DIM "[", stdout);

    /* 2. 填充部分：根据百分比变色 (绿/黄/红) */
    if (pct > 80.0f)
    {
        fputs(COLOR_RED, stdout);
    }
    else if (pct > 50.0f)
    {
        fputs(COLOR_YELLOW, stdout);
    }
    else
    {
        fputs(COLOR_GREEN, stdout);
    }

    for (int i = 0; i < filled; i++)
    {
        fputs("■", stdout);
    }

    /* 3. 未填充部分：强制重置填充色，设为暗灰色 */
    fputs(COLOR_RESET COLOR_DIM, stdout);
    for (int i = filled; i < width - 2; i++)
    {
        fputs("░", stdout);
    }

    /* 4. 右括号：保持暗灰色，并彻底重置颜色状态 */
    fputs("]" COLOR_RESET, stdout);
}

/* ── 绘制全宽面板框 ── */
static void draw_panel(int row, int h, int w, const char *title)
{
    if (h < 2 || w < 4)
    {
        return;
    }
    /* 顶边框：彩色标题 */
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
    if (remain < 0)
    {
        remain = 0;
    }
    fputs(COLOR_DIM, stdout);
    phoriz(remain);
    fputs(TR, stdout);
    fputs(COLOR_RESET, stdout);

    /* 侧边 */
    for (int r = 1; r < h - 1; r++)
    {
        gotoxy(row + r, 1);
        fputs(COLOR_DIM VERT COLOR_RESET, stdout);
        gotoxy(row + r, w);
        fputs(COLOR_DIM VERT COLOR_RESET, stdout);
    }

    /* 底边框 */
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

    /* 1. 先开启备用屏幕缓冲区，瞬间切走原始终端内容 */
    printf("\033[?1049h");
    fflush(stdout);

    /* 2. 关闭终端回显和行缓冲，防止键盘输入显示在屏幕上 */
    tcgetattr(STDIN_FILENO, &orig_termios);
    struct termios new_termios = orig_termios;
    new_termios.c_lflag &= ~(ECHO | ICANON); // 关闭回显(ECHO)和规范模式(ICANON)
    new_termios.c_cc[VMIN] = 0;  // 关键：read 不等待，立即返回
    new_termios.c_cc[VTIME] = 0; // 关键：无超时
    tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);
    tcflush(STDIN_FILENO, TCIFLUSH); // 清空输入缓冲区残留

    /* 3. 隐藏光标并清屏 */
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

    /* 检测终端大小变化，如果发生变化则清屏消除残影 */
    if (term_cols != prev_term_cols || term_rows != prev_term_rows)
    {
        fputs("\033[2J", stdout);
        prev_term_cols = term_cols;
        prev_term_rows = term_rows;
    }

    int w = term_cols;
    int cur_row = 1;
    char buf[256];

    /* ── 标题栏 ── */
    gotoxy(cur_row, 1);
    fputs("\033[K", stdout);
    const char *title_str = " INTEL CORE ULTRA SOC MONITOR ";
    int x = (w - (int)strlen(title_str)) / 2;
    if (x < 1)
    {
        x = 1;
    }
    gotoxy(cur_row, x);
    fputs(COLOR_BOLD COLOR_MAGENTA, stdout);
    fputs(title_str, stdout);
    fputs(COLOR_RESET, stdout);
    cur_row += 2;

    /* ── 动态计算进度条宽度 (终端宽度 - 文本占位 - 边框) ── */
    int bar_w = w - 55;
    if (bar_w > 40)
    {
        bar_w = 40;
    }
    if (bar_w < 10)
    {
        bar_w = 10;
    }

    /* ── CPU 面板 ── */
    int cpu_h = 9;
    draw_panel(cur_row, cpu_h, w, "CPU (P-Core / E-Core)");
    if (cpu)
    {
        gotoxy(cur_row + 1, 3);
        fputs(COLOR_YELLOW, stdout);
        snprintf(buf, sizeof(buf), "P-Core:%5.1fMHz IPC:%.2f", cpu->pcore_freq_mhz, cpu->pcore_ipc);
        fputs(buf, stdout);
        fputs(COLOR_RESET, stdout);
        gotoxy(cur_row + 1, 35);
        draw_bar((float)cpu->pcore_cache_miss_rate, bar_w);
        snprintf(buf, sizeof(buf), " L3:%5.2f%%", cpu->pcore_cache_miss_rate);
        fputs(buf, stdout);
        fputs(COLOR_RESET "\033[K", stdout);

        gotoxy(cur_row + 2, 3);
        snprintf(buf, sizeof(buf), "      Br Miss:%5.2f%%", cpu->pcore_branch_miss_rate);
        fputs(buf, stdout);
        fputs(COLOR_RESET "\033[K", stdout);

        gotoxy(cur_row + 4, 3);
        fputs(COLOR_BLUE, stdout);
        snprintf(buf, sizeof(buf), "E-Core:%5.1fMHz IPC:%.2f", cpu->ecore_freq_mhz, cpu->ecore_ipc);
        fputs(buf, stdout);
        fputs(COLOR_RESET, stdout);
        gotoxy(cur_row + 4, 35);
        draw_bar((float)cpu->ecore_cache_miss_rate, bar_w);
        snprintf(buf, sizeof(buf), " L3:%5.2f%%", cpu->ecore_cache_miss_rate);
        fputs(buf, stdout);
        fputs(COLOR_RESET "\033[K", stdout);

        gotoxy(cur_row + 5, 3);
        snprintf(buf, sizeof(buf), "      Br Miss:%5.2f%%", cpu->ecore_branch_miss_rate);
        fputs(buf, stdout);
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
            snprintf(buf, sizeof(buf), "热裕度: %3d ℃", cpu->thermal_margin_degrees);
            fputs(buf, stdout);
            fputs(COLOR_RESET, stdout);
        }
        else
        {
            fputs("热裕度: N/A" COLOR_RESET, stdout);
        }
        fputs("\033[K", stdout);
    }
    else
    {
        gotoxy(cur_row + 4, 3);
        fputs(COLOR_DIM "(disabled)" COLOR_RESET "\033[K", stdout);
    }
    gotoxy(cur_row + cpu_h, 1); fputs("\033[K", stdout); // 清理面板间空行
    cur_row += cpu_h + 1;

    /* ── GPU & NPU 面板 ── */
    int gpu_h = 7;
    draw_panel(cur_row, gpu_h, w, "Xe Graphics & NPU");
    if (gpu)
    {
        gotoxy(cur_row + 1, 3);
        snprintf(buf, sizeof(buf), "渲染占用: %5.1f%% ", gpu->utilization);
        fputs(buf, stdout);
        draw_bar((float)gpu->utilization, 30);
        fputs(COLOR_RESET "\033[K", stdout);

        gotoxy(cur_row + 2, 3);
        fputs(COLOR_GREEN, stdout);
        snprintf(buf, sizeof(buf), "视频解码: %5.1f%% ", gpu->video_utilization);
        fputs(buf, stdout);
        fputs(COLOR_RESET, stdout);
        draw_bar((float)gpu->video_utilization, 30);
        fputs(COLOR_RESET "\033[K", stdout);

        gotoxy(cur_row + 3, 3);
        fputs(COLOR_CYAN, stdout);
        snprintf(buf, sizeof(buf), "显存占用: %6.2f MiB (共享系统内存)", gpu->vram_used_mib);
        fputs(buf, stdout);
        fputs(COLOR_RESET "\033[K", stdout);
    }
    else
    {
        gotoxy(cur_row + 2, 3);
        fputs(COLOR_DIM "(disabled)" COLOR_RESET "\033[K", stdout);
    }

    /* ── NPU 渲染 ── */
    if (npu)
    {
        gotoxy(cur_row + 4, 3);
        fputs(COLOR_MAGENTA, stdout);
        snprintf(buf, sizeof(buf), "AI 引擎 (NPU): %5.1f%% ", npu->utilization_pct);
        fputs(buf, stdout);
        fputs(COLOR_RESET, stdout);
        draw_bar((float)npu->utilization_pct, 20);
        snprintf(buf, sizeof(buf), " 频率: %4.0f / %4.0f MHz", npu->freq_mhz, npu->max_freq_mhz);
        fputs(buf, stdout);
        fputs(COLOR_RESET "\033[K", stdout);

        gotoxy(cur_row + 5, 3);
        fputs(COLOR_CYAN, stdout);
        snprintf(buf, sizeof(buf), "NPU 显存占用: %6.2f MiB", npu->mem_mib);
        fputs(buf, stdout);
        fputs(COLOR_RESET "\033[K", stdout);
    }
    else
    {
        gotoxy(cur_row + 4, 3);
        fputs(COLOR_DIM "(NPU disabled)" COLOR_RESET "\033[K", stdout);
    }
    gotoxy(cur_row + gpu_h, 1); fputs("\033[K", stdout); // 清理面板间空行
    cur_row += gpu_h + 1;

    /* ── SOC 面板 (内存 + 功耗) ── */
    int soc_h = 7;
    draw_panel(cur_row, soc_h, w, "SOC (Memory & Power & Battery)");
    if (mem_cap || mem || power || battery)
    {
        int row_off = 1;
        /* 第一行：RAM 容量 */
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
            snprintf(buf, sizeof(buf), "MEM 读取: %7.2f MiB/s", mem->read_mib_s);
            fputs(buf, stdout);
            fputs(COLOR_RESET, stdout);
            gotoxy(cur_row + row_off, 35);
            fputs(COLOR_BLUE, stdout);
            snprintf(buf, sizeof(buf), "写入: %7.2f MiB/s", mem->write_mib_s);
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
        }
        if (battery)
        {
            gotoxy(cur_row + row_off, 3);
            fputs(COLOR_CYAN, stdout);

            const char *status_text = "Unknown";
            if (battery->status == BATTERY_STATUS_CHARGING)
            {
                status_text = "Charging";
            }
            else if (battery->status == BATTERY_STATUS_DISCHARGING)
            {
                status_text = "Discharging";
            }
            else if (battery->status == BATTERY_STATUS_FULL)
            {
                status_text = "Full";
            }

            snprintf(buf, sizeof(buf), "BAT %s %3d%% %5.2fW", status_text, battery->capacity, battery->watts);
            fputs(buf, stdout);
            fputs(COLOR_RESET, stdout);

            // 电池容量进度条
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
    gotoxy(cur_row + soc_h, 1); fputs("\033[K", stdout); // 清理面板间空行
    cur_row += soc_h + 1;

    /* ── IO 面板 (Disk & Network) ── */
    int io_h = 4;
    draw_panel(cur_row, io_h, w, "IO (Disk & Network)");
    if (disk || net)
    {
        int row_off = 1;
        if (disk)
        {
            gotoxy(cur_row + row_off, 3);
            fputs(COLOR_MAGENTA, stdout);
            snprintf(buf, sizeof(buf), "DSK 读取: %7.2f MiB/s", disk->read_mib_s);
            fputs(buf, stdout);
            fputs(COLOR_RESET, stdout);
            gotoxy(cur_row + row_off, 35);
            fputs(COLOR_RED, stdout);
            snprintf(buf, sizeof(buf), "写入: %7.2f MiB/s", disk->write_mib_s);
            fputs(buf, stdout);
            fputs(COLOR_RESET "\033[K", stdout);
            row_off++;
        }
        if (net)
        {
            gotoxy(cur_row + row_off, 3);
            fputs(COLOR_CYAN, stdout);
            snprintf(buf, sizeof(buf), "NET 下载: %7.2f MiB/s", net->rx_mib_s);
            fputs(buf, stdout);
            fputs(COLOR_RESET, stdout);
            gotoxy(cur_row + row_off, 35);
            fputs(COLOR_GREEN, stdout);
            snprintf(buf, sizeof(buf), "上传: %7.2f MiB/s", net->tx_mib_s);
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
    gotoxy(cur_row + io_h, 1); fputs("\033[K", stdout); // 清理面板间空行
    cur_row += io_h + 1;

    /* ── 清除内容下方的残留行 ── */
    for (int r = cur_row; r <= term_rows; r++)
    {
        gotoxy(r, 1);
        fputs("\033[K", stdout);
    }

    /* ── 底部提示 ── */
    gotoxy(term_rows, 1);
    fputs(COLOR_DIM " Press Ctrl+C to quit " COLOR_RESET, stdout);
    fputs("\033[K", stdout);
    fflush(stdout);
}

void display_cleanup(void)
{
    /* 恢复原始终端输入属性 */
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);

    /* 关闭备用屏幕缓冲区，恢复原始终端内容 */
    printf("\033[?25h\033[2J\033[H\033[?1049l");
    fflush(stdout);
}
