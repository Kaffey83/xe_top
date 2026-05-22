#include "npu_monitor.h"
#include <stdio.h>
#include <stdlib.h>

#define NPU_DEV_PATH "/sys/class/accel/accel0/device"

static int npu_available = 0;

int npu_monitor_init(void)
{
    /* 检查核心文件是否可读，判断 NPU 驱动是否加载 */
    char path[256];
    snprintf(path, sizeof(path), "%s/npu_busy_time_us", NPU_DEV_PATH);
    FILE *f = fopen(path, "r");
    if (!f)
    {
        npu_available = 0;
        return -1;
    }
    fclose(f);
    npu_available = 1;
    return 0;
}

int npu_monitor_read(npu_stats_t *stats)
{
    if (!stats || !npu_available)
    {
        return -1;
    }

    char path[256];
    FILE *f;

    /* 读取累计忙碌时间 (微秒) */
    snprintf(path, sizeof(path), "%s/npu_busy_time_us", NPU_DEV_PATH);
    f = fopen(path, "r");
    if (!f) return -1;
    if (fscanf(f, "%llu", &stats->busy_time_us) != 1)
    {
        fclose(f);
        return -1;
    }
    fclose(f);

    /* 读取当前频率 */
    snprintf(path, sizeof(path), "%s/npu_current_frequency_mhz", NPU_DEV_PATH);
    f = fopen(path, "r");
    if (!f) return -1;
    if (fscanf(f, "%u", &stats->freq_mhz) != 1)
    {
        fclose(f);
        return -1;
    }
    fclose(f);

    /* 读取最大频率 */
    snprintf(path, sizeof(path), "%s/npu_max_frequency_mhz", NPU_DEV_PATH);
    f = fopen(path, "r");
    if (!f) return -1;
    if (fscanf(f, "%u", &stats->max_freq_mhz) != 1)
    {
        fclose(f);
        return -1;
    }
    fclose(f);

    /* 读取显存占用 (字节) */
    snprintf(path, sizeof(path), "%s/npu_memory_utilization", NPU_DEV_PATH);
    f = fopen(path, "r");
    if (!f) return -1;
    if (fscanf(f, "%llu", &stats->mem_bytes) != 1)
    {
        fclose(f);
        return -1;
    }
    fclose(f);

    return 0;
}

int npu_monitor_compute(const npu_stats_t *prev, const npu_stats_t *cur, double elapsed_sec, npu_metrics_t *out)
{
    if (!prev || !cur || !out || elapsed_sec <= 0.0)
    {
        return -1;
    }

    /* 计算占用率: (忙碌时间差 / 总流逝时间) * 100% */
    long long delta_busy = (long long)(cur->busy_time_us - prev->busy_time_us);
    
    /* 防御无符号减法下溢 */
    if (delta_busy < 0) delta_busy = 0;

    double total_elapsed_us = elapsed_sec * 1000000.0;
    out->utilization_pct = (delta_busy / total_elapsed_us) * 100.0;
    
    if (out->utilization_pct > 100.0)
    {
        out->utilization_pct = 100.0;
    }

    out->freq_mhz = (double)cur->freq_mhz;
    out->max_freq_mhz = (double)cur->max_freq_mhz;
    out->mem_mib = (double)cur->mem_bytes / (1024.0 * 1024.0);

    return 0;
}

void npu_monitor_cleanup(void)
{
    /* 无需清理 */
}
