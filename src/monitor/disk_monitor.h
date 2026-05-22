#ifndef DISK_MONITOR_H
#define DISK_MONITOR_H

#include <stdint.h>

/* 磁盘状态快照 (累计字节数) */
typedef struct
{
    unsigned long long read_bytes;
    unsigned long long write_bytes;
} disk_stats_t;

/* 磁盘派生指标 (速率) */
typedef struct
{
    double read_mib_s;
    double write_mib_s;
} disk_metrics_t;

int disk_monitor_init(void);
int disk_monitor_read(disk_stats_t *stats);
int disk_monitor_compute(const disk_stats_t *prev, const disk_stats_t *cur, double elapsed_sec, disk_metrics_t *out);
void disk_monitor_cleanup(void);

#endif /* DISK_MONITOR_H */
