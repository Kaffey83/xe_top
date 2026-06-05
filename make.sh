#!/bin/bash
#
# xe_top — Intel Core Ultra Full-Stack Performance Monitor Build Script
# Builds the project using CMake
#

set -e

# ── Color definitions ──
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

PROJECT="xe_top"

echo -e "${CYAN}========================================${NC}"
echo -e "${CYAN}   ${PROJECT} — Intel Core Ultra SOC Monitor${NC}"
echo -e "${CYAN}========================================${NC}"
echo ""

# ── Check dependencies ──
if ! command -v cmake &> /dev/null; then
    echo -e "${RED}[ERROR] CMake is not installed${NC}"
    echo "  Ubuntu/Debian: sudo apt install cmake"
    echo "  Fedora:        sudo dnf install cmake"
    echo "  Arch:          sudo pacman -S cmake"
    exit 1
fi

CC="${CC:-gcc}"
if ! command -v "$CC" &> /dev/null; then
    echo -e "${RED}[ERROR] Compiler '$CC' not found${NC}"
    exit 1
fi

if ! command -v make &> /dev/null; then
    echo -e "${RED}[ERROR] make is not installed${NC}"
    exit 1
fi

echo -e "${GREEN}  Compiler: $(command -v "$CC")"
echo -e "  CMake:    $(cmake --version | head -n1)${NC}"
echo ""

# ── Build ──
BUILD_DIR="build"

echo -e "${YELLOW}==> Cleaning old build files...${NC}"
rm -rf "$BUILD_DIR"

echo -e "${GREEN}==> Running CMake configure...${NC}"
cmake -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}" \
    -DCMAKE_C_COMPILER="$CC" \
    ${CMAKE_OPTS}

echo -e "${GREEN}==> Building project...${NC}"
cd "$BUILD_DIR"
make -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2)" 2>&1

EXEC="./${PROJECT}"

if [ -f "$EXEC" ]; then
    echo ""
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}  Build successful!${NC}"
    echo -e "${GREEN}  Executable: $(pwd)/${PROJECT}${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo ""
    echo -e "${YELLOW}Usage examples:${NC}"
    echo ""
    echo "  # Full monitoring (sudo recommended, some features require root)"
    echo -e "  ${CYAN}sudo ./${PROJECT}${NC}"
    echo ""
    echo "  # Custom refresh interval"
    echo -e "  ${CYAN}sudo ./${PROJECT} -i 2${NC}"
    echo ""
    echo "  # Disable specific modules"
    echo -e "  ${CYAN}sudo ./${PROJECT} --no-gpu --no-cpu --no-power${NC}"
    echo ""
    echo "  # Show help"
    echo -e "  ${CYAN}./${PROJECT} -h${NC}"
    echo ""
    echo -e "${YELLOW}Monitor modules:${NC}"
    echo "  GPU (Xe)  ·  CPU (P/E-Core)  ·  Memory (BW+Cap)  ·  Power (RAPL)"
    echo "  Battery   ·  Disk             ·  Network           ·  NPU"
else
    echo -e "${RED}[ERROR] Build failed, ${PROJECT} not found${NC}"
    exit 1
fi