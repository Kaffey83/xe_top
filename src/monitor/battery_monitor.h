#ifndef BATTERY_MONITOR_H
#define BATTERY_MONITOR_H

#include <stdint.h>
#include <stdbool.h>

/* 电池状态枚举 */
typedef enum
{
    BATTERY_STATUS_UNKNOWN = 0,
    BATTERY_STATUS_CHARGING,
    BATTERY_STATUS_DISCHARGING,
    BATTERY_STATUS_FULL
} battery_status_t;

/* 电池指标 */
typedef struct
{
    int capacity;               /* 剩余电量百分比 (0-100) */
    battery_status_t status;    /* 充放状态 */
    double watts;               /* 当前瞬时功率 (瓦特 W) */
} battery_metrics_t;

/* 初始化电池监控 (检查 sysfs 可用性) */
int battery_monitor_init(void);

/* 读取并计算当前电池指标 */
int battery_monitor_read(battery_metrics_t *metrics);

/* 清理 (空操作) */
void battery_monitor_cleanup(void);

#endif /* BATTERY_MONITOR_H */
