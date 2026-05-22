#ifndef DISPLAY_H
#define DISPLAY_H

#include "../monitor/gpu_monitor.h"
#include "../monitor/cpu_monitor.h"
#include "../monitor/power_monitor.h"
#include "../monitor/mem_monitor.h"
#include "../monitor/battery_monitor.h" // 新增
#include "../monitor/disk_monitor.h"
#include "../monitor/net_monitor.h"
#include "../monitor/npu_monitor.h"

void display_init(void);
void display_render(const gpu_metrics_t *gpu, const cpu_metrics_t *cpu, const mem_stats_t *mem, const mem_capacity_t *mem_cap, const power_metrics_t *power, const battery_metrics_t *battery, const disk_metrics_t *disk, const net_metrics_t *net, const npu_metrics_t *npu);
void display_cleanup(void);

#endif /* DISPLAY_H */