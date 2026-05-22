#ifndef NPU_MONITOR_H
#define NPU_MONITOR_H

#include <stdint.h>

/* NPU 状态快照 */
typedef struct {
    unsigned long long busy_time_us;
    unsigned int freq_mhz;
    unsigned int max_freq_mhz;
    unsigned long long mem_bytes;
} npu_stats_t;

/* NPU 派生指标 */
typedef struct {
    double utilization_pct;
    double freq_mhz;
    double max_freq_mhz;
    double mem_mib;
} npu_metrics_t;

int npu_monitor_init(void);
int npu_monitor_read(npu_stats_t *stats);
int npu_monitor_compute(const npu_stats_t *prev, const npu_stats_t *cur, double elapsed_sec, npu_metrics_t *out);
void npu_monitor_cleanup(void);

#endif /* NPU_MONITOR_H */
