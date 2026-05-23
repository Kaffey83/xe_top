# xe_top

<div align="center">

**Intel Core Ultra Full-Stack Performance Monitoring Tool**

Real-time monitoring of GPU · CPU · Memory(Bandwidth+Capacity) · Power(RAPL) · Battery · Disk · Network · NPU

Full-screen terminal UI similar to `top`, data from Linux PMU / sysfs, consistent with Ubuntu System Monitor logic

[![License](https://img.shields.io/badge/license-GPLv2-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Linux-brightgreen.svg)](https://www.kernel.org/)
[![Language](https://img.shields.io/badge/language-C11-orange.svg)](https://en.cppreference.com/wiki/c/help/idx/intro)
[![CPU](https://img.shields.io/badge/CPU-Intel%20Core%20Ultra-blueviolet)](https://www.intel.com/)

</div>

---

## Features

| Module | Data Source | Metrics |
|--------|-------------|---------|
| **Xe GPU** | Kernel PMU driver | Render engine utilization (%), Video engine utilization (%), Actual frequency (MHz), VRAM usage (MiB) |
| **CPU** | Hardware performance counters | P-Core / E-Core actual frequency (MHz), IPC, Cache miss rate (%), Branch miss rate (%), C6/C10 deep sleep percentage (%), Thermal margin (°C) |
| **Memory Bandwidth** | Uncore IMC counters | Read/Write bandwidth (MiB/s, GB/s) |
| **Memory Capacity** | `/proc/meminfo` | Total / Used / Available (GiB), Usage (%), based on MemAvailable (includes reclaimable cache, consistent with System Monitor) |
| **RAPL Power** | Kernel MSR/sysfs | Package / Core / DRAM power (W) |
| **Battery** | ACPI sysfs | Charge/discharge status, Remaining capacity (%), Instantaneous power (W) |
| **Disk I/O** | `/sys/block` | Read/Write rate (MiB/s) |
| **Network I/O** | `/sys/class/net` | Download/Upload rate (MiB/s) |
| **NPU (AI Engine)** | sysfs device nodes | Utilization (%), Actual/Max frequency (MHz), Memory usage (MiB) |

## System Requirements

| Item | Requirement |
|------|-------------|
| **CPU** | Intel Core Ultra (200V / 200H / 200U series) |
| **OS** | Linux kernel ≥ 6.8 (recommended) |
| **Permissions** | Some metrics require `root` (RAPL power, PMU counters) |
| **Dependencies** | `cmake`, `gcc`/`clang`, `make`, librt, libm (system standard libraries) |

## Quick Start

### Build

```bash
# Using project script (recommended)
chmod +x make.sh
./make.sh

# Or build manually with CMake
cmake -B build && cmake --build build
```

Build output is located at `build/xe_top`.

### Run

```bash
# Full monitoring (sudo recommended, some features require root)
sudo ./build/xe_top

# Custom refresh interval (2 seconds)
sudo ./build/xe_top -i 2

# Disable specific modules
sudo ./build/xe_top --no-gpu --no-cpu --no-power

# Monitor only CPU + GPU (no root required)
./build/xe_top --no-power --no-battery --no-disk --no-net

# Show help
./build/xe_top -h
```

### Keyboard Shortcuts

| Key | Function |
|-----|----------|
| `Ctrl+C` | Exit program (via signal handling + non-blocking stdin read) |

## Command-Line Options

```
Usage: ./xe_top [OPTIONS]

Options:
  -i, --interval <sec>  Refresh interval (default: 1, supports sub-second, minimum 0.1)
  -G, --no-gpu         Disable GPU monitoring
  -C, --no-cpu         Disable CPU monitoring
  -P, --no-power       Disable power monitoring
  -M, --no-memory      Disable memory monitoring (bandwidth + capacity)
  -B, --no-battery     Disable battery monitoring
  -D, --no-disk        Disable disk monitoring
  -N, --no-net         Disable network monitoring
  -A, --no-npu         Disable NPU monitoring
  -h, --help           Display this help message
```

## Project Structure

```
├── CMakeLists.txt              # CMake build configuration (C11, -Wall -Werror)
├── make.sh                     # Quick build script (auto-detects dependencies, colored output)
├── README.md                   # Project documentation
├── src/
│   ├── main.c                  # Program entry and main loop (signal handling + non-blocking stdin)
│   ├── config/
│   │   ├── config.h            # Runtime configuration structure (module enable flags, interval)
│   │   └── config.c            # getopt_long command-line argument parsing (supports sub-second interval)
│   ├── monitor/
│   │   ├── gpu_monitor.c/h     # Xe GPU PMU monitoring (dynamically finds xe_ PMU devices)
│   │   ├── cpu_monitor.c/h     # CPU PMU monitoring (P-Core / E-Core separate counting)
│   │   ├── mem_monitor.c/h     # Memory bandwidth (IMC counters) + capacity (/proc/meminfo)
│   │   ├── power_monitor.c/h   # RAPL power monitoring (Package / Core / DRAM)
│   │   ├── battery_monitor.c/h # Battery monitoring (dynamically scans ACPI sysfs battery devices)
│   │   ├── disk_monitor.c/h    # Disk I/O monitoring (auto-skips loop/zram)
│   │   ├── net_monitor.c/h     # Network I/O monitoring (auto-selects first up NIC, skips lo)
│   │   └── npu_monitor.c/h     # NPU AI engine monitoring (/sys/class/accel/accel0)
│   ├── display/
│   │   ├── display.h           # Terminal UI declaration
│   │   └── display.c           # ANSI full-screen rendering engine (alternate screen buffer)
│   └── util/
│       ├── perf_util.h         # PMU utility function declarations
│       └── perf_util.c         # perf_event_open wrapper / sysfs parsing
└── build/                      # Build output directory
    └── xe_top                  # Executable
```

## UI Layout

```
┌─ INTEL CORE ULTRA SOC MONITOR ─────────────────────┐
│                                                     │
│ ┌─ CPU (P-Core / E-Core) ────────────────────────┐ │
│ │ P-Core: 3800.0MHz IPC:1.23  [■■■■■■■■░░░░] L3: 2.10% │
│ │       Br Miss: 0.50%                            │ │
│ │ E-Core: 2500.0MHz IPC:0.89  [■■■■■■░░░░░░] L3: 1.80% │
│ │       Br Miss: 0.30%                            │ │
│ │ Core C6: 45.0% [■■■■■■■■░░] Pkg C10: 30.0% [■■■■■■░░] │
│ │ Thermal Margin: 45 ℃                            │ │
│ └─────────────────────────────────────────────────┘ │
│                                                     │
│ ┌─ Xe Graphics & NPU ────────────────────────────┐ │
│ │ Render:  23.5% [■■■■■■■■■■■■■■■■■■░░░░░░░░]       │ │
│ │ Video:    5.2% [■■■■░░░░░░░░░░░░░░░░░░░░░░]       │ │
│ │ VRAM: 456.78 MiB (shared system memory)          │ │
│ │ AI Engine (NPU):  12.3% [■■■■■■░░░░] Freq: 800 / 1400 MHz │
│ │ NPU Memory: 128.00 MiB                           │ │
│ └─────────────────────────────────────────────────┘ │
│                                                     │
│ ┌─ SOC (Memory & Power & Battery) ───────────────┐ │
│ │ RAM:  3.45 / 15.31 GiB [■■■░░░░░░░░░░░]  22.5%   │ │
│ │ MEM Read: 1234.56 MiB/s  Write:  567.89 MiB/s   │ │
│ │ PWR Pkg: 12.34 W  Core:  5.67 W  DRAM: 1.23 W   │ │
│ │ BAT Discharging  78%  [■■■■■■■■■■■■░░░░░░] 4.56W │ │
│ └─────────────────────────────────────────────────┘ │
│                                                     │
│ ┌─ IO (Disk & Network) ──────────────────────────┐ │
│ │ DSK Read:   45.67 MiB/s  Write:  23.45 MiB/s     │ │
│ │ NET Download: 1.23 MiB/s  Upload: 0.45 MiB/s     │ │
│ └─────────────────────────────────────────────────┘ │
│                                                     │
│              Press Ctrl+C to quit                    │
└─────────────────────────────────────────────────────┘
```

## Technical Implementation

### Data Collection

| Module | Technical Path | Details |
|--------|---------------|---------|
| **GPU** | `perf_event_open` → Xe PMU | Dynamically scan `/sys/bus/event_source/devices/` for `xe_` prefix PMU devices; parse `event` / `engine_class` / `engine_instance` / `gt` field bit offsets from sysfs; open render engine and video decode engine active-ticks / total-ticks counters; poll `/proc/*/fdinfo` to aggregate GTT VRAM usage by drm-client-id (scan every 3 sampling cycles to reduce overhead) |
| **CPU** | `perf_event_open` → Architectural PMU | Bind P-Core (`cpu_core`) and E-Core (`cpu_atom`) separately to `instructions`, `cycles`, `ref-cycles` events; read `aperf`/`mperf` via MSR PMU to calculate actual frequency; read C6/C10 residency via `cstate_core`/`cstate_pkg` PMU; read thermal margin via MSR thermal event |
| **Memory Bandwidth** | `perf_event_open` → Uncore IMC | Open `uncore_imc_free_running_0/1` `data_read`/`data_write` events (parse `event=,umask=` dual-field config); auto-read `.scale` factor from sysfs; accumulate delta from both IMC channels |
| **Memory Capacity** | Read `/proc/meminfo` | Parse `MemTotal` / `MemAvailable`, real used = Total - Available (includes reclaimable cache, consistent with System Monitor), unit kB → MiB → GiB |
| **Power** | Read RAPL sysfs | Read Package / Core / DRAM energy counters (µJ) from `/sys/class/powercap/intel-rapl:0`; calculate power (W) from delta, guard against unsigned underflow |
| **Battery** | Read ACPI sysfs | Dynamically scan `/sys/class/power_supply/` for `type=Battery` devices; calculate instantaneous power (W) from `current_now` (µA) × `voltage_now` (µV); take absolute value, direction from `status` |
| **Disk** | Read `/sys/block/*/stat` | Auto-skip `loop` and `zram` virtual devices, select first real block device; parse stat file fields 3/7 (read/write sector count); Linux kernel fixed 512 bytes/sector, delta for rate |
| **Network** | Read `/sys/class/net/*/statistics` | Auto-skip `lo` loopback interface, select first physical NIC with `operstate=up`; parse `rx_bytes` / `tx_bytes`, delta for up/down rate |
| **NPU** | Read sysfs device nodes | Read `npu_busy_time_us` (cumulative busy microseconds), `npu_current_frequency_mhz`, `npu_max_frequency_mhz`, `npu_memory_utilization` from `/sys/class/accel/accel0/device/`; utilization = (busy time delta / elapsed time) × 100% |

### Display Engine

- Use **ANSI escape sequences** for full-screen overlay UI (no flicker, no clear screen)
- **Alternate screen buffer**: Save terminal content on entry with `\033[?1049h`, restore on exit with `\033[?1049l`
- **Non-blocking keyboard input**: `tcsetattr` to disable echo (ECHO) and canonical mode (ICANON), set `VMIN=0, VTIME=0` for immediate `read()` return; main loop reads and discards stdin buffer each iteration to prevent input buildup
- **Signal handling**: Capture `SIGINT` / `SIGTERM` / `SIGHUP`, set `running=0` for graceful exit and terminal restore
- **Adaptive terminal size**: Get terminal width/height via `ioctl TIOCGWINSZ` each render; clear screen on size change to eliminate ghosting
- **btop-style dynamic progress bars**: Clear line end with `\033[K`, fill characters `■` (filled) and `░` (empty); auto-color by percentage: ≤50% green → 80% yellow → >80% red
- **UTF-8 box characters** (`┌─┐│└─┘`) for panel drawing, support `(disabled)` placeholder when module disabled

## Build Customization

### CMake Options

```bash
# Specify compiler
CC=clang cmake -B build -DCMAKE_BUILD_TYPE=Release

# Debug mode
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# Specify architecture optimization
cmake -B build -DCMAKE_C_FLAGS="-O2 -march=native"
```

### Build Script Variables

```bash
# Specify compiler
CC=clang ./make.sh

# Pass CMake options
CMAKE_OPTS="-DCMAKE_C_FLAGS=-O2" ./make.sh
```

## Known Issues

### NPU Frequency Read Failure
NPU actual and maximum frequency show as 0 or abnormal values. This is an Intel NPU driver issue and should be resolved when the driver is updated.

### Video Engine Utilization Unavailable
Xe GPU video decode engine utilization cannot currently be read, fix in progress.

### More Features Coming Soon
- Support for other Core Ultra series (e.g., 200H, 200U)
- More hardware monitoring metrics
- Historical data recording and export

---

## Troubleshooting

### Insufficient Permissions

```bash
# Run with sudo
sudo ./build/xe_top

# Or add user to perfmon group (some systems)
sudo usermod -a -G perfmon $USER
```

### Some Metrics Show 0 or Disabled

```bash
# Check if PMU is available
sudo dmesg | grep -i perf

# Check kernel modules
lsmod | grep -E "i915|xe|intel_rapl"
lsmod | grep -E "accel"  # NPU driver

# Confirm PMU devices exist
ls /sys/bus/event_source/devices/
```

### Memory Bandwidth Shows 0

```bash
# Confirm Uncore IMC PMU exists
ls /sys/bus/event_source/devices/ | grep uncore_imc

# Check IMC event files
cat /sys/bus/event_source/devices/uncore_imc_free_running_0/events/data_read
```

### Memory Capacity Shows "(RAM disabled)"

```bash
# Confirm /proc/meminfo is readable
cat /proc/meminfo | head -5
```

### NPU Unavailable

```bash
# Check NPU device nodes
ls /sys/class/accel/

# Confirm NPU driver loaded
ls /sys/class/accel/accel0/device/
```

### Terminal Display Issues

```bash
# Ensure terminal supports ANSI 256 colors
export TERM=xterm-256color

# Reset terminal (if program exits abnormally leaving terminal in bad state)
reset
```

### Program Abnormal Exit Causes Terminal State Issues

If program exits without cleanup (e.g., `kill -9`), the following issues may remain:
- **Echo disabled**: Input not visible → Run `stty echo` to restore
- **Cursor hidden**: Cursor disappeared → Run `printf '\033[?25h'` to show cursor
- **Alternate screen stuck** → Run `printf '\033[?1049l'` to exit alternate screen

Running `reset` directly can restore all terminal attributes at once.

## License

This project is released under the [GPL-2.0](LICENSE) license.

## Contributing

Issues and Pull Requests are welcome!

---

<div align="center">

**Made with ❤️ for Linux performance enthusiasts**

</div>