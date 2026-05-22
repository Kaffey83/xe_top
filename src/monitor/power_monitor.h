#ifndef POWER_MONITOR_H
#define POWER_MONITOR_H

#include <stdint.h>

/* 电源状态快照（能量单位为微焦耳 µJ）*/
typedef struct
{
    unsigned long long pkg_energy;
    unsigned long long core_energy;
    unsigned long long dram_energy;
} power_stats_t;

/* 以瓦特为单位的派生电源指标 */
typedef struct
{
    double pkg_watts;
    double core_watts;
    double dram_watts;
} power_metrics_t;

/*
 * 初始化 RAPL 电源监控子系统。
 * 验证 RAPL sysfs 文件是否可访问。
 * 成功返回 0，失败返回 -1。
 */
int power_monitor_init(void);

/*
 * 读取当前原始 RAPL 能量计数器。
 * 成功返回 0，失败返回 -1。
 */
int power_monitor_read(power_stats_t *stats);

/*
 * 由两个能量快照和经过时间计算功率（瓦特）。
 * 成功返回 0，无效输入返回 -1。
 */
int power_monitor_compute(const power_stats_t *prev, const power_stats_t *cur,
                          double elapsed_sec, power_metrics_t *out);

/*
 * 清理（对于 sysfs 为空操作，但提供以保持对称性）。
 */
void power_monitor_cleanup(void);

#endif /* POWER_MONITOR_H */