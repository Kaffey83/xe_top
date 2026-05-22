#include "perf_util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <asm/unistd.h>

long perf_util_open(struct perf_event_attr *hw_event, pid_t pid, int cpu, int group_fd, unsigned long flags)
{
    return syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
}

int perf_util_get_pmu_type(const char *pmu_name)
{
    char path[128];
    snprintf(path, sizeof(path), "/sys/bus/event_source/devices/%s/type", pmu_name);

    FILE *f = fopen(path, "r");
    if (!f)
    {
        return -1;
    }

    int type = 0;
    if (fscanf(f, "%d", &type) != 1)
    {
        type = -1;
    }
    fclose(f);
    return type;
}

int perf_util_parse_format(const char *pmu_path, const char *field,
                           int default_offset, int default_length)
{
    char path[256];
    snprintf(path, sizeof(path), "%s/format/%s", pmu_path, field);

    FILE *f = fopen(path, "r");
    if (!f)
    {
        return (default_offset << 8) | default_length;
    }

    char buf[64];
    if (!fgets(buf, sizeof(buf), f))
    {
        fclose(f);
        return (default_offset << 8) | default_length;
    }
    fclose(f);

    char *colon = strchr(buf, ':');
    if (!colon)
    {
        return (default_offset << 8) | default_length;
    }

    int start = 0, end = 0;
    if (sscanf(colon + 1, "%d-%d", &start, &end) == 2)
    {
        return (start << 8) | (end - start + 1);
    }

    return (default_offset << 8) | default_length;
}

unsigned long long perf_util_parse_event_code(const char *pmu_path, const char *event_name)
{
    char path[256];
    snprintf(path, sizeof(path), "%s/events/%s", pmu_path, event_name);

    FILE *f = fopen(path, "r");
    if (!f)
    {
        return 0;
    }

    char buf[64];
    if (!fgets(buf, sizeof(buf), f))
    {
        fclose(f);
        return 0;
    }
    fclose(f);

    unsigned long long code = 0;
    char *eq = strchr(buf, '=');
    if (eq)
    {
        sscanf(eq + 1, "%llx", &code);
    }
    else
    {
        /* 某些 PMU（如 uncore IMC）直接存储十六进制代码 */
        sscanf(buf, "%llx", &code);
    }

    return code;
}

unsigned long long perf_util_build_config(unsigned long long *fmt_info,
                                          unsigned long long *vals,
                                          int num_fields)
{
    unsigned long long config = 0;
    for (int i = 0; i < num_fields; i++)
    {
        int offset = fmt_info[i] >> 8;
        config |= (vals[i] << offset);
    }
    return config;
}

int perf_util_get_first_cpu(const char *pmu_name)
{
    char path[256];
    /* 尝试 PMU 特定的 cpus 文件 */
    snprintf(path, sizeof(path), "/sys/bus/event_source/devices/%s/cpus", pmu_name);
    FILE *f = fopen(path, "r");
    if (f)
    {
        char buf[64];
        if (fgets(buf, sizeof(buf), f))
        {
            char *p = buf;
            while (*p && (*p < '0' || *p > '9')) p++;
            if (*p)
            {
                int cpu = atoi(p);
                fclose(f);
                return cpu;
            }
        }
        fclose(f);
    }
    /* 回退：尝试 Xe 风格 PMU 的设备 cpumask */
    snprintf(path, sizeof(path), "/sys/devices/%s/cpumask", pmu_name);
    f = fopen(path, "r");
    if (f)
    {
        char buf[16];
        if (fgets(buf, sizeof(buf), f))
        {
            int cpu = atoi(buf);
            fclose(f);
            return cpu;
        }
        fclose(f);
    }
    return -1;
}

int perf_util_open_event(int pmu_type, unsigned long long config, const char *pmu_path)
{
    struct perf_event_attr pea;
    memset(&pea, 0, sizeof(pea));

    pea.size = sizeof(pea);
    pea.type = pmu_type;
    pea.config = config;
    pea.disabled = 0;
    pea.inherit = 1;

    /* 确定要绑定的 CPU */
    int cpu = 0; /* 默认为 CPU 0 */
    if (pmu_path)
    {
        char path[256];
        snprintf(path, sizeof(path), "%s/cpumask", pmu_path);
        FILE *f = fopen(path, "r");
        if (f)
        {
            char buf[16];
            if (fgets(buf, sizeof(buf), f))
            {
                cpu = atoi(buf);
            }
            fclose(f);
        }
    }

    int fd = perf_util_open(&pea, -1, cpu, -1, 0);

    /* 如果失败且有 pmu_path，尝试从 sysfs 读取 type */
    if (fd == -1 && pmu_path)
    {
        char path[256];
        snprintf(path, sizeof(path), "%s/type", pmu_path);
        FILE *f = fopen(path, "r");
        if (f)
        {
            char buf[16];
            if (fgets(buf, sizeof(buf), f))
            {
                pea.type = atoi(buf);
                fd = perf_util_open(&pea, -1, cpu, -1, 0);
            }
            fclose(f);
        }
    }

    return fd;
}

int perf_util_read_counter(int fd, unsigned long long *value)
{
    if (fd < 0 || !value)
    {
        return -1;
    }

    long long val = 0;
    if (read(fd, &val, sizeof(val)) != sizeof(val))
    {
        return -1;
    }

    *value = (unsigned long long)val;
    return 0;
}