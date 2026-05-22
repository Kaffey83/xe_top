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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

static volatile sig_atomic_t running = 1;

/* 统一处理退出信号，确保终端状态恢复 */
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

    /* 捕获中断 (Ctrl+C)、终止 和 挂起 (关闭终端) 信号 */
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
}

int main(int argc, char *argv[])
{
    /* --- 解析配置 --- */
    runtime_config_t cfg;
    config_parse(argc, argv, &cfg);

    if (cfg.help_requested)
    {
        return 0;
    }

    /* --- 初始化请求的监控模块 --- */
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
        fprintf(stderr, "所有监控模块初始化均失败，退出。\n");
        goto cleanup; /* 跳转到清理逻辑，避免直接退出导致资源泄漏 */
    }

    /* --- 初始化全屏显示与信号处理 --- */
    display_init();
    setup_signals();

    /* --- 初始采样 --- */
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

    /* --- 主循环 --- */
    while (running)
    {
        /* 非阻塞地读取并丢弃用户误输入的字符，防止输入缓冲区满 */
        char discard_buf[64];
        ssize_t n_read;
        do {
            n_read = read(STDIN_FILENO, discard_buf, sizeof(discard_buf));
        } while (n_read > 0 || (n_read < 0 && errno == EINTR));

        /* 亚秒级休眠 (兼容 int 和 double 类型的 interval_sec) */
        double interval = (double)cfg.interval_sec;
        struct timespec req = {
            .tv_sec = (time_t)interval,
            .tv_nsec = (long)((interval - (time_t)interval) * 1e9)
        };
        nanosleep(&req, NULL);

        /* 读取当前采样值 */
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

        /* --- 计算指标 --- */
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

        /* --- 渲染全屏 UI --- */
        display_render(gpu_ok ? &gpu_metrics : NULL,
                       cpu_ok ? &cpu_metrics : NULL,
                       mem_ok ? &cur_mem : NULL,
                       mem_cap_ok ? &mem_cap : NULL,
                       pwr_ok ? &power_metrics : NULL,
                       bat_ok ? &cur_bat : NULL,
                       dsk_ok ? &disk_metrics : NULL,
                       net_ok ? &net_metrics : NULL,
                       npu_ok ? &npu_metrics : NULL);

        /* --- 保存状态供下一次迭代使用 --- */
        if (gpu_ok) prev_gpu = cur_gpu;
        if (cpu_ok) prev_cpu = cur_cpu;
        if (pwr_ok) prev_pwr = cur_pwr;
        if (dsk_ok) prev_dsk = cur_dsk;
        if (net_ok) prev_net = cur_net;
        if (npu_ok) prev_npu = cur_npu;

        ts_start = ts_end;
    }

cleanup:
    /* --- 清理 --- */
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
}
