# Contributing to xe_top

Thank you for your interest in contributing to xe_top!

## Prerequisites

- **OS**: Linux kernel 6.18+ (Xe GPU driver merged in 6.18), Ubuntu 25.10+
- **Compiler**: GCC 14+ or Clang 18+
- **Build**: CMake 3.10+, make
- **Runtime**: Most PMU/RAPL features require root (`sudo`)

## Development Setup

```bash
git clone https://github.com/Kaffey83/xe_top.git && cd xe_top
./make.sh          # or: cmake -B build && cmake --build build
cd build && sudo ./xe_top
```

## Running Tests

```bash
cd build && ./test_common && ./test_ring
```

## Code Style

- C11 standard with POSIX extensions
- 4-space indentation, no tabs
- `snake_case` for functions and variables
- `SCREAMING_SNAKE` for macros and constants
- GPL-2.0 SPDX header on every source file
- `static` for module-internal functions/variables
- Every monitor follows `init / read / compute / cleanup` lifecycle
- Use `DELTA_SAFE()` for all counter delta calculations
- Use paths from `paths.h` for sysfs paths

## Adding a New Monitor

1. Create `src/monitor/<name>_monitor.h` and `<name>_monitor.c`
2. Define `<name>_stats_t` (raw snapshot) and `<name>_metrics_t` (derived metrics)
3. Implement `init / read / compute / cleanup` functions
4. Add to `CMakeLists.txt` source list
5. Add enable flag to `config.h` / `config.c`
6. Wire into `main.c` and `display.c`

## Pull Request Process

1. Fork the repository
2. Create a feature branch: `git checkout -b feature/my-feature`
3. Make changes and ensure `make` succeeds with `-Werror`
4. Add tests for new functionality
5. Update CHANGELOG.md
6. Submit PR with a clear description

## Bug Reports

Please include:
- Kernel version (`uname -r`)
- CPU model (`lscpu | grep "Model name"`)
- Whether running with `sudo`
- Error output (run without TUI redirection: comment out `freopen` in `main.c`)