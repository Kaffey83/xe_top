# xe_top

<div align="center">

**Intel Core Ultra Full-Stack Performance Monitor**

Real-time TUI monitoring of GPU · CPU · Memory · Power · Battery · Disk · Network · NPU

[![License](https://img.shields.io/badge/license-GPL--2.0-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Linux%20x86__64-brightgreen.svg)](https://www.kernel.org/)
[![Language](https://img.shields.io/badge/language-C11-orange.svg)](https://en.cppreference.com/w/c/11)
[![CPU](https://img.shields.io/badge/CPU-Intel%20Core%20Ultra-blueviolet)](https://www.intel.com/)

[English](./README.md) | [中文](./README.zh.md)

</div>

---

## Features

| Module | Data Source | Metrics |
|--------|-------------|---------|
| **Xe GPU** | Kernel PMU (`perf_event_open`) | Render engine %, Video engine %, Frequency (MHz), VRAM (MiB) |
| **CPU** | Architectural PMU + MSR | P-Core / E-Core frequency (MHz), IPC, L3 miss %, Branch miss %, C6/C10 %, Thermal margin (°C) |
| **Memory BW** | Uncore IMC counters | Read / Write bandwidth (MiB/s) |
| **Memory Cap** | `/proc/meminfo` | Total / Used / Available (GiB), Usage % (includes reclaimable cache) |
| **Power** | RAPL sysfs (cached fd) | Package / Core / DRAM power (W) |
| **Battery** | ACPI sysfs | Status, Capacity %, Power (W) |
| **Disk I/O** | `/sys/block/*/stat` (cached fd) | Read / Write rate (MiB/s) |
| **Network** | `/sys/class/net/*/statistics` (cached fd) | Download / Upload rate (MiB/s) |
| **NPU** | `/sys/class/accel/accel0/device/` (cached fd) | Utilization %, Frequency (MHz), Memory (MiB) |

## System Requirements

| Item | Requirement |
|------|-------------|
| **CPU** | Intel Core Ultra (200V / 200H / 200U series) |
| **OS** | Linux kernel ≥ 6.18 (Xe GPU driver merged into mainline in 6.18) |
| **Distro** | Ubuntu 25.10+ (or any distro with kernel 6.18+) |
| **Permissions** | Most metrics require `root` (PMU counters, RAPL energy) |
| **Dependencies** | `cmake` ≥ 3.10, `gcc` ≥ 14 / `clang` ≥ 18, `make`, librt, libm |

## Quick Start

### Build

```bash
# Using build script (recommended)
chmod +x make.sh && ./make.sh

# Or manually with CMake
cmake -B build && cmake --build build
```

Executable: `build/xe_top`

### Run

```bash
# Full monitoring (sudo recommended)
sudo ./build/xe_top

# Custom refresh interval (0.5 seconds)
sudo ./build/xe_top -i 0.5

# Disable specific modules
sudo ./build/xe_top --no-gpu --no-cpu --no-power

# CPU + GPU only (no root needed for some metrics)
./build/xe_top --no-power --no-battery --no-disk --no-net

# Show version
./build/xe_top --version

# Show help
./build/xe_top -h
```

### Interactive Keys

| Key | Function |
|-----|----------|
| `q` / `Esc` | Quit |
| `+` / `-` | Adjust refresh interval (±0.1s) |
| `p` | Pause / Resume data collection |
| `h` | Toggle help overlay |

## Command-Line Options

```
Usage: xe_top [OPTIONS]

Options:
  -i, --interval <sec>  Refresh interval (default: 1, range: 0.1-60)
  -G, --no-gpu         Disable GPU monitoring
  -C, --no-cpu         Disable CPU monitoring
  -P, --no-power       Disable power monitoring
  -M, --no-memory      Disable memory monitoring (bandwidth + capacity)
  -B, --no-battery     Disable battery monitoring
  -D, --no-disk        Disable disk monitoring
  -N, --no-net         Disable network monitoring
  -A, --no-npu         Disable NPU monitoring
  -V, --version        Show version number
  -h, --help           Show this help message
```

## Project Structure

```
├── CMakeLists.txt          # CMake build (C11, -Wall -Wextra -Wpedantic -Werror)
├── make.sh                 # Build script (auto-detect deps, colored output)
├── src/
│   ├── main.c              # Entry point, main loop, signal handling, key dispatch
│   ├── config/
│   │   ├── config.h        # Runtime config struct (module flags, interval)
│   │   └── config.c        # getopt_long argument parsing, interval validation
│   ├── monitor/
│   │   ├── paths.h         # Centralized sysfs paths and tunable constants
│   │   ├── gpu_monitor.c/h # Xe GPU PMU (dynamic xe_ device scan, GTT VRAM)
│   │   ├── cpu_monitor.c/h # CPU PMU (P-Core/E-Core, MSR freq, cstate, thermal)
│   │   ├── mem_monitor.c/h # Memory BW (IMC counters) + capacity (/proc/meminfo)
│   │   ├── power_monitor.c/h # RAPL power (cached fd + pread)
│   │   ├── battery_monitor.c/h # Battery (dynamic ACPI sysfs scan)
│   │   ├── disk_monitor.c/h  # Disk I/O (cached fd, auto-skip loop/zram)
│   │   ├── net_monitor.c/h   # Network I/O (cached fd, auto-select up NIC)
│   │   └── npu_monitor.c/h   # NPU AI engine (cached fd, graceful degradation)
│   ├── display/
│   │   ├── display.h       # TUI render API
│   │   ├── display.c       # Full-screen renderer (panels, bars, sparklines, help)
│   │   ├── style.h         # ANSI colors, box-drawing chars, sparkline chars
│   │   └── term.h          # Terminal control abstraction
│   └── util/
│       ├── common.h        # DELTA_SAFE, clamp_ll, MIN/MAX, ARRAY_SIZE macros
│       ├── ring.h          # Ring buffer for sparkline history (60 samples)
│       ├── version.h       # Version macros (XE_TOP_VERSION_STRING)
│       ├── perf_util.h     # PMU helper declarations
│       └── perf_util.c     # perf_event_open wrapper, sysfs PMU parsing
├── tests/
│   ├── test_common.c       # Unit tests for common.h macros
│   └── test_ring.c         # Unit tests for ring buffer
├── docs/
│   └── xe_top.1            # Man page
├── .github/workflows/ci.yml # CI (Ubuntu 25.10, multi-compiler matrix)
└── .clang-tidy             # Static analysis config
```

## UI Layout

![screenshot](./images/screenshot.png)

## Technical Details

### Data Collection

- **GPU**: Dynamically scans `/sys/bus/event_source/devices/` for `xe_` PMU devices; parses event/engine_class/engine_instance/gt bit offsets from sysfs; opens render + video engine active/total ticks counters; polls `/proc/*/fdinfo` for GTT VRAM by drm-client-id (every 3 cycles)
- **CPU**: Binds P-Core (`cpu_core`) and E-Core (`cpu_atom`) to instructions/cycles/ref-cycles; reads aperf/mperf via MSR for actual frequency; reads C6/C10 via cstate PMU; reads thermal margin via MSR
- **Memory BW**: Opens `uncore_imc_free_running_0/1` data_read/data_write events; auto-reads `.scale` factor; accumulates both IMC channels
- **Memory Cap**: Parses `/proc/meminfo` MemTotal/MemAvailable; used = Total - Available (consistent with System Monitor)
- **Power**: Reads RAPL energy counters via cached fd + `pread`; computes watts from delta with `DELTA_SAFE` underflow protection
- **Battery**: Dynamically scans `/sys/class/power_supply/` for battery devices; power = |current_now × voltage_now|
- **Disk/NPU/Net**: All use cached file descriptors opened at init, read via `pread` each cycle

### Display Engine

- ANSI escape sequences for full-screen overlay UI (no flicker)
- Alternate screen buffer (`\033[?1049h` / `\033[?1049l`)
- Non-blocking stdin with `tcsetattr` (ECHO off, ICANON off, VMIN=0)
- Signal handling (SIGINT/SIGTERM/SIGHUP → graceful exit)
- Adaptive terminal size via `ioctl TIOCGWINSZ`
- btop-style progress bars with auto-color (≤50% green → 80% yellow → >80% red)
- Sparkline history rings (60 samples, 8-level `▁▂▃▄▅▆▇█`)
- Help overlay toggled by `h` key
- stderr redirected to `/dev/null` to prevent TUI corruption

## Build Options

```bash
# Release build (default)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Debug build
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# With AddressSanitizer
cmake -B build -DENABLE_ASAN=ON

# With UBSan
cmake -B build -DENABLE_UBSAN=ON

# Install
sudo cmake --install build --prefix=/usr/local

# Run tests
cd build && ./test_common && ./test_ring
```

## Known Issues

- **NPU frequency**: Shows `N/A (driver unsupported)` when driver doesn't report freq info (Intel NPU driver bug)
- **Video engine**: Shows `(unsupported)` when video decode PMU counters are unavailable

## Troubleshooting

```bash
# Check PMU availability
ls /sys/bus/event_source/devices/ | grep -E "xe_|cpu_core|cpu_atom|uncore_imc"

# Check kernel modules
lsmod | grep -E "xe|intel_rapl|accel"

# Check RAPL
cat /sys/class/powercap/intel-rapl:0/energy_uj

# Check NPU
ls /sys/class/accel/accel0/device/

# Reset terminal after abnormal exit
reset
```

## License

[GPL-2.0](LICENSE)