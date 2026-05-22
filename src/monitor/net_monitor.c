#include "net_monitor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#define SYS_NET_PATH "/sys/class/net"

static char rx_path[512] = {0};
static char tx_path[512] = {0};

/* 动态查找活跃的物理网卡 (跳过 lo) */
static int find_active_net(void)
{
    DIR *dir = opendir(SYS_NET_PATH);
    if (!dir)
    {
        return -1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        /* 跳过自身、上级目录和回环接口 */
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0 ||
            strcmp(entry->d_name, "lo") == 0)
        {
            continue;
        }

        char state_path[512];
        snprintf(state_path, sizeof(state_path), "%s/%s/operstate", SYS_NET_PATH, entry->d_name);

        FILE *f = fopen(state_path, "r");
        if (f)
        {
            char state[16] = {0};
            if (fgets(state, sizeof(state), f))
            {
                /* 找到第一个状态为 up 的网卡 */
                if (strncmp(state, "up", 2) == 0)
                {
                    snprintf(rx_path, sizeof(rx_path), "%s/%s/statistics/rx_bytes", SYS_NET_PATH, entry->d_name);
                    snprintf(tx_path, sizeof(tx_path), "%s/%s/statistics/tx_bytes", SYS_NET_PATH, entry->d_name);
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

int net_monitor_init(void)
{
    if (find_active_net() != 0)
    {
        return -1;
    }
    return 0;
}

int net_monitor_read(net_stats_t *stats)
{
    if (!stats || rx_path[0] == '\0')
    {
        return -1;
    }

    FILE *f_rx = fopen(rx_path, "r");
    FILE *f_tx = fopen(tx_path, "r");
    
    if (!f_rx || !f_tx)
    {
        if (f_rx) fclose(f_rx);
        if (f_tx) fclose(f_tx);
        return -1;
    }

    if (fscanf(f_rx, "%llu", &stats->rx_bytes) != 1) stats->rx_bytes = 0;
    if (fscanf(f_tx, "%llu", &stats->tx_bytes) != 1) stats->tx_bytes = 0;

    fclose(f_rx);
    fclose(f_tx);
    return 0;
}

int net_monitor_compute(const net_stats_t *prev, const net_stats_t *cur, double elapsed_sec, net_metrics_t *out)
{
    if (!prev || !cur || !out || elapsed_sec <= 0.0)
    {
        return -1;
    }

    double delta_rx = (double)(cur->rx_bytes - prev->rx_bytes);
    double delta_tx = (double)(cur->tx_bytes - prev->tx_bytes);

    out->rx_mib_s = (delta_rx / (1024.0 * 1024.0)) / elapsed_sec;
    out->tx_mib_s = (delta_tx / (1024.0 * 1024.0)) / elapsed_sec;
    return 0;
}

void net_monitor_cleanup(void)
{
    /* 无需清理 */
}
