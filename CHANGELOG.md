# Changelog

All notable changes to xe_top will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.0] - 2025-06-05

### Added
- Full-stack SOC monitoring: CPU (P-Core/E-Core), GPU (Xe, requires kernel 6.18+), Power (RAPL), Memory (IMC), Battery, Disk, Network, NPU
- btop-style TUI with progress bars, panel layout, and sparkline history rings
- Dynamic PMU/sysfs device discovery (no hardcoded device names)
- Interactive keys: `q`/Esc to quit, `+`/`-` to adjust interval, `p` to pause, `h` for help
- `--version` flag showing version number
- `--interval` option with proper validation (strtod + range 0.1–60s)
- Module disable flags (`--no-gpu`, `--no-cpu`, `--no-power`, `--no-memory`, `--no-battery`, `--no-disk`, `--no-net`, `--no-npu`)
- Sparkline ring buffers (60 samples) for CPU frequency, GPU utilization, NPU utilization, and power
- Responsive layout computation adapting bar/sparkline widths to terminal size
- Bottom help bar with keyboard shortcuts
- `isatty()` guard to prevent running in non-terminal environments
- stderr redirected to `/dev/null` to avoid TUI corruption
- Centralized path constants in `paths.h`
- Common utility macros (`DELTA_SAFE`, `clamp_ll`, `MIN`, `MAX`, `ARRAY_SIZE`) in `common.h`
- Ring buffer utility (`ring.h`) for sparkline data
- Display style constants extracted to `style.h`
- Version macros in `version.h`
- CMake install rules, CPack packaging (DEB/RPM/TGZ)
- Sanitizer options (ASan/UBSan) and hardening flags
- Unit tests for `common.h` and `ring.h` utilities
- GitHub Actions CI workflow with multi-compiler matrix
- Man page (`docs/xe_top.1`)
- CHANGELOG.md and CONTRIBUTING.md

### Fixed
- Memory monitor first-cycle zero display - now seeds `prev_raw_*` from initial counter reads
- `thermal_margin` type changed from `unsigned long long` to `long long` to correctly handle signed MSR values
- NPU frequency graceful degradation - shows "N/A (driver unsupported)" when `max_freq_mhz == 0`
- Video engine shows "(unsupported)" when PMU fds are unavailable
- GPU frequency display restored in the UI
- Interval parameter validation with `strtod` + range limits (0.1–60s)

### Changed
- `DELTA_SAFE` macro for counter wrap-around protection across all monitors
- Cached sysfs file descriptors in power, NPU, disk, and net monitors (using `pread` instead of `fopen/fclose` per cycle)
- All code comments and UI text converted to English
- Kernel requirement updated to 6.18+ (Xe GPU driver merged in 6.18)
- CI updated to Ubuntu 25.10 with gcc-14/15, clang-18/19/20