#ifndef NET_MONITOR_H
#define NET_MONITOR_H

#include <stdint.h>

/* 网络状态快照 (累计字节数) */
typedef struct
{
    unsigned long long rx_bytes;
    unsigned long long tx_bytes;
} net_stats_t;

/* 网络派生指标 (速率) */
typedef struct
{
    double rx_mib_s;
    double tx_mib_s;
} net_metrics_t;

int net_monitor_init(void);
int net_monitor_read(net_stats_t *stats);
int net_monitor_compute(const net_stats_t *prev, const net_stats_t *cur, double elapsed_sec, net_metrics_t *out);
void net_monitor_cleanup(void);

#endif /* NET_MONITOR_H */
