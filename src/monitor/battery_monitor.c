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


#include "battery_monitor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#define POWER_SUPPLY_PATH "/sys/class/power_supply"

static char bat_path[512] = {0};

/* 动态查找系统中的电池设备 (跳过 AC) */
static int find_battery(void)
{
    DIR *dir = opendir(POWER_SUPPLY_PATH);
    if (!dir)
    {
        return -1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        char type_path[512];
        snprintf(type_path, sizeof(type_path), "%s/%s/type", POWER_SUPPLY_PATH, entry->d_name);

        FILE *f = fopen(type_path, "r");
        if (f)
        {
            char type[32] = {0};
            if (fgets(type, sizeof(type), f))
            {
                if (strncmp(type, "Battery", 7) == 0)
                {
                    snprintf(bat_path, sizeof(bat_path), "%s/%s", POWER_SUPPLY_PATH, entry->d_name);
                    fclose(f);
                    closedir(dir);
                    return 0;
                }
            }
            fclose(f);
        }
    }
    closedir(dir);
    return -1;
}

/* 读取 sysfs 中的整数值 */
static int read_sysfs_int(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f)
    {
        return -1;
    }
    int val = 0;
    if (fscanf(f, "%d", &val) != 1)
    {
        val = -1;
    }
    fclose(f);
    return val;
}

/* 读取 sysfs 中的电池状态字符串 */
static battery_status_t read_sysfs_status(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f)
    {
        return BATTERY_STATUS_UNKNOWN;
    }

    char buf[32] = {0};
    battery_status_t status = BATTERY_STATUS_UNKNOWN;
    if (fgets(buf, sizeof(buf), f))
    {
        if (strncmp(buf, "Charging", 8) == 0)
        {
            status = BATTERY_STATUS_CHARGING;
        }
        else if (strncmp(buf, "Discharging", 11) == 0)
        {
            status = BATTERY_STATUS_DISCHARGING;
        }
        else if (strncmp(buf, "Full", 4) == 0)
        {
            status = BATTERY_STATUS_FULL;
        }
    }
    fclose(f);
    return status;
}

int battery_monitor_init(void)
{
    if (find_battery() != 0)
    {
        return -1;
    }
    return 0;
}

int battery_monitor_read(battery_metrics_t *metrics)
{
    if (!metrics || bat_path[0] == '\0')
    {
        return -1;
    }

    char path[1024];

    snprintf(path, sizeof(path), "%s/capacity", bat_path);
    metrics->capacity = read_sysfs_int(path);

    snprintf(path, sizeof(path), "%s/status", bat_path);
    metrics->status = read_sysfs_status(path);

    snprintf(path, sizeof(path), "%s/current_now", bat_path);
    int current_ua = read_sysfs_int(path);

    snprintf(path, sizeof(path), "%s/voltage_now", bat_path);
    int voltage_uv = read_sysfs_int(path);

    if (current_ua >= 0 && voltage_uv > 0)
    {
        /* 功率 W = 电流 A * 电压 V = (µA * 1e-6) * (µV * 1e-6) */
        double power_w = ((double)current_ua / 1000000.0) * ((double)voltage_uv / 1000000.0);
        /* 某些驱动放电电流为正，有些为负，统一取绝对值，方向由 status 判断 */
        metrics->watts = (power_w > 0) ? power_w : -power_w;
    }
    else
    {
        metrics->watts = 0.0;
    }

    /* 容错保护 */
    if (metrics->capacity < 0)
    {
        metrics->capacity = 0;
    }
    if (metrics->capacity > 100)
    {
        metrics->capacity = 100;
    }

    return 0;
}

void battery_monitor_cleanup(void)
{
    /* sysfs 读取无需清理 */
}
