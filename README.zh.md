# xe_top

<div align="center">

**Intel Core Ultra 全栈性能监控工具**

实时 TUI 监控 GPU · CPU · 内存 · 功耗 · 电池 · 磁盘 · 网络 · NPU

[![License](https://img.shields.io/badge/license-GPL--2.0-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Linux%20x86__64-brightgreen.svg)](https://www.kernel.org/)
[![Language](https://img.shields.io/badge/language-C11-orange.svg)](https://en.cppreference.com/w/c/11)
[![CPU](https://img.shields.io/badge/CPU-Intel%20Core%20Ultra-blueviolet)](https://www.intel.com/)

[English](./README.md) | [中文](./README.zh.md)

</div>

---

## 功能特性

| 模块 | 数据来源 | 指标 |
|------|---------|------|
| **Xe GPU** | 内核 PMU (`perf_event_open`) | 渲染引擎占用 %, 视频引擎占用 %, 频率 (MHz), 显存 (MiB) |
| **CPU** | 架构 PMU + MSR | P-Core / E-Core 频率 (MHz), IPC, L3 未命中 %, 分支未命中 %, C6/C10 %, 热裕度 (°C) |
| **内存带宽** | Uncore IMC 计数器 | 读 / 写带宽 (MiB/s) |
| **内存容量** | `/proc/meminfo` | 总量 / 已用 / 可用 (GiB), 使用率 % (含可回收缓存) |
| **功耗** | RAPL sysfs (缓存 fd) | Package / Core / DRAM 功耗 (W) |
| **电池** | ACPI sysfs | 状态, 电量 %, 功率 (W) |
| **磁盘 IO** | `/sys/block/*/stat` (缓存 fd) | 读 / 写速率 (MiB/s) |
| **网络** | `/sys/class/net/*/statistics` (缓存 fd) | 下载 / 上传速率 (MiB/s) |
| **NPU** | `/sys/class/accel/accel0/device/` (缓存 fd) | 利用率 %, 频率 (MHz), 显存 (MiB) |

## 系统要求

| 项目 | 要求 |
|------|------|
| **CPU** | Intel Core Ultra (200V / 200H / 200U 系列) |
| **操作系统** | Linux 内核 ≥ 6.18（Xe GPU 驱动于 6.18 合入主线） |
| **发行版** | Ubuntu 25.10+（或任何内核 6.18+ 的发行版） |
| **权限** | 大部分指标需 `root`（PMU 计数器、RAPL 能量） |
| **依赖** | `cmake` ≥ 3.10, `gcc` ≥ 14 / `clang` ≥ 18, `make`, librt, libm |

## 快速开始

### 编译

```bash
# 使用构建脚本（推荐）
chmod +x make.sh && ./make.sh

# 或手动 CMake
cmake -B build && cmake --build build
```

可执行文件：`build/xe_top`

### 运行

```bash
# 完整监控（推荐 sudo）
sudo ./build/xe_top

# 自定义刷新间隔（0.5 秒）
sudo ./build/xe_top -i 0.5

# 禁用特定模块
sudo ./build/xe_top --no-gpu --no-cpu --no-power

# 仅 CPU + GPU（部分指标无需 root）
./build/xe_top --no-power --no-battery --no-disk --no-net

# 显示版本
./build/xe_top --version

# 显示帮助
./build/xe_top -h
```

### 交互按键

| 按键 | 功能 |
|------|------|
| `q` / `Esc` | 退出 |
| `+` / `-` | 调整刷新间隔（±0.1s） |
| `p` | 暂停 / 继续数据采集 |
| `h` | 切换帮助覆盖层 |

## 命令行选项

```
用法: xe_top [选项]

选项:
  -i, --interval <秒>  刷新间隔 (默认: 1, 范围: 0.1-60)
  -G, --no-gpu         禁用 GPU 监控
  -C, --no-cpu         禁用 CPU 监控
  -P, --no-power       禁用功耗监控
  -M, --no-memory      禁用内存监控（带宽 + 容量）
  -B, --no-battery     禁用电池监控
  -D, --no-disk        禁用磁盘监控
  -N, --no-net         禁用网络监控
  -A, --no-npu         禁用 NPU 监控
  -V, --version        显示版本号
  -h, --help           显示帮助信息
```

## 项目结构

```
├── CMakeLists.txt          # CMake 构建 (C11, -Wall -Wextra -Wpedantic -Werror)
├── make.sh                 # 构建脚本（自动检测依赖、彩色输出）
├── src/
│   ├── main.c              # 入口、主循环、信号处理、按键分发
│   ├── config/
│   │   ├── config.h        # 运行时配置结构体（模块开关、间隔）
│   │   └── config.c        # getopt_long 参数解析、间隔校验
│   ├── monitor/
│   │   ├── paths.h         # 集中管理的 sysfs 路径和可调常量
│   │   ├── gpu_monitor.c/h # Xe GPU PMU（动态 xe_ 设备扫描、GTT 显存）
│   │   ├── cpu_monitor.c/h # CPU PMU（P-Core/E-Core、MSR 频率、cstate、热裕度）
│   │   ├── mem_monitor.c/h # 内存带宽（IMC 计数器）+ 容量 (/proc/meminfo)
│   │   ├── power_monitor.c/h # RAPL 功耗（缓存 fd + pread）
│   │   ├── battery_monitor.c/h # 电池（动态 ACPI sysfs 扫描）
│   │   ├── disk_monitor.c/h  # 磁盘 IO（缓存 fd、自动跳过 loop/zram）
│   │   ├── net_monitor.c/h   # 网络 IO（缓存 fd、自动选择 up 网卡）
│   │   └── npu_monitor.c/h   # NPU AI 引擎（缓存 fd、优雅降级）
│   ├── display/
│   │   ├── display.h       # TUI 渲染 API
│   │   ├── display.c       # 全屏渲染器（面板、进度条、sparkline、帮助）
│   │   ├── style.h         # ANSI 颜色、框线字符、sparkline 字符
│   │   └── term.h          # 终端控制抽象层
│   └── util/
│       ├── common.h        # DELTA_SAFE、clamp_ll、MIN/MAX、ARRAY_SIZE 宏
│       ├── ring.h          # 环形缓冲区，sparkline 历史（60 采样）
│       ├── version.h       # 版本宏 (XE_TOP_VERSION_STRING)
│       ├── perf_util.h     # PMU 辅助函数声明
│       └── perf_util.c     # perf_event_open 封装、sysfs PMU 解析
├── tests/
│   ├── test_common.c       # common.h 宏单元测试
│   └── test_ring.c         # 环形缓冲区单元测试
├── docs/
│   └── xe_top.1            # 手册页
├── .github/workflows/ci.yml # CI（Ubuntu 25.10、多编译器矩阵）
└── .clang-tidy             # 静态分析配置
```

## UI 布局

![截图](./images/screenshot.png)

## 技术实现

### 数据采集

- **GPU**：动态扫描 `/sys/bus/event_source/devices/` 查找 `xe_` PMU 设备；从 sysfs 解析 event/engine_class/engine_instance/gt 字段位偏移；打开渲染+视频引擎 active/total ticks 计数器；轮询 `/proc/*/fdinfo` 按 drm-client-id 累加 GTT 显存（每 3 周期）
- **CPU**：分别绑定 P-Core (`cpu_core`) 和 E-Core (`cpu_atom`) 的 instructions/cycles/ref-cycles；通过 MSR 读取 aperf/mperf 计算实际频率；通过 cstate PMU 读取 C6/C10 驻留；通过 MSR 读取热裕度
- **内存带宽**：打开 `uncore_imc_free_running_0/1` data_read/data_write 事件；自动读取 `.scale` 缩放因子；累计两个 IMC 通道
- **内存容量**：解析 `/proc/meminfo` 的 MemTotal/MemAvailable；已用 = Total - Available（与系统监视器一致）
- **功耗**：通过缓存 fd + `pread` 读取 RAPL 能量计数器；用 `DELTA_SAFE` 差分计算瓦特，防御下溢
- **电池**：动态扫描 `/sys/class/power_supply/` 查找电池设备；功率 = |current_now × voltage_now|
- **磁盘/NPU/网络**：均在 init 时打开缓存 fd，每周期用 `pread` 读取

### 显示引擎

- ANSI 转义序列实现全屏覆盖式 UI（无闪烁）
- 备用屏幕缓冲区（`\033[?1049h` / `\033[?1049l`）
- 非阻塞 stdin：`tcsetattr` 关闭 ECHO/ICANON，VMIN=0
- 信号处理（SIGINT/SIGTERM/SIGHUP → 优雅退出）
- 自适应终端尺寸：`ioctl TIOCGWINSZ`
- btop 风格进度条，自动变色（≤50% 绿 → 80% 黄 → >80% 红）
- Sparkline 历史环形缓冲（60 采样，8 级 `▁▂▃▄▅▆▇█`）
- `h` 键切换帮助覆盖层
- stderr 重定向至 `/dev/null` 防止 TUI 损坏

## 构建选项

```bash
# Release 构建（默认）
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Debug 构建
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# 启用 AddressSanitizer
cmake -B build -DENABLE_ASAN=ON

# 启用 UBSan
cmake -B build -DENABLE_UBSAN=ON

# 安装
sudo cmake --install build --prefix=/usr/local

# 运行测试
cd build && ./test_common && ./test_ring
```

## 已知问题

- **NPU 频率**：驱动未报告频率信息时显示 `N/A (driver unsupported)`（Intel NPU 驱动 bug）
- **视频引擎**：视频解码 PMU 计数器不可用时显示 `(unsupported)`

## 故障排除

```bash
# 检查 PMU 可用性
ls /sys/bus/event_source/devices/ | grep -E "xe_|cpu_core|cpu_atom|uncore_imc"

# 检查内核模块
lsmod | grep -E "xe|intel_rapl|accel"

# 检查 RAPL
cat /sys/class/powercap/intel-rapl:0/energy_uj

# 检查 NPU
ls /sys/class/accel/accel0/device/

# 异常退出后重置终端
reset
```

## 许可证

[GPL-2.0](LICENSE)