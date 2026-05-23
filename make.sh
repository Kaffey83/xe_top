#!/bin/bash
#
# xe_top — Intel Core Ultra 全栈性能监控工具 构建脚本
# 使用 CMake 编译项目
#

set -e

# ── 颜色定义 ──
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

PROJECT="xe_top"

echo -e "${CYAN}========================================${NC}"
echo -e "${CYAN}   ${PROJECT} — Intel Core Ultra 全栈监控工具${NC}"
echo -e "${CYAN}========================================${NC}"
echo ""

# ── 检查依赖 ──
if ! command -v cmake &> /dev/null; then
    echo -e "${RED}[错误] CMake 未安装${NC}"
    echo "  Ubuntu/Debian: sudo apt install cmake"
    echo "  Fedora:        sudo dnf install cmake"
    echo "  Arch:          sudo pacman -S cmake"
    exit 1
fi

CC="${CC:-gcc}"
if ! command -v "$CC" &> /dev/null; then
    echo -e "${RED}[错误] 编译器 '$CC' 未找到${NC}"
    exit 1
fi

if ! command -v make &> /dev/null; then
    echo -e "${RED}[错误] make 未安装${NC}"
    exit 1
fi

echo -e "${GREEN}  编译器: $(command -v "$CC")"
echo -e "  CMake:   $(cmake --version | head -n1)${NC}"
echo ""

# ── 构建 ──
BUILD_DIR="build"

echo -e "${YELLOW}==> 清理旧的构建文件...${NC}"
rm -rf "$BUILD_DIR"

echo -e "${GREEN}==> 执行 CMake 配置...${NC}"
cmake -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}" \
    -DCMAKE_C_COMPILER="$CC" \
    ${CMAKE_OPTS}

echo -e "${GREEN}==> 编译项目...${NC}"
cd "$BUILD_DIR"
make -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2)" 2>&1

EXEC="./${PROJECT}"

if [ -f "$EXEC" ]; then
    echo ""
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}  ✓ 编译成功！${NC}"
    echo -e "${GREEN}  可执行文件: $(pwd)/${PROJECT}${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo ""
    echo -e "${YELLOW}用法示例:${NC}"
    echo ""
    echo "  # 完整监控（推荐 sudo，部分功能需 root）"
    echo -e "  ${CYAN}sudo ./${PROJECT}${NC}"
    echo ""
    echo "  # 自定义刷新间隔"
    echo -e "  ${CYAN}sudo ./${PROJECT} -i 2${NC}"
    echo ""
    echo "  # 禁用特定模块"
    echo -e "  ${CYAN}sudo ./${PROJECT} --no-gpu --no-cpu --no-power${NC}"
    echo ""
    echo "  # 查看帮助"
    echo -e "  ${CYAN}./${PROJECT} -h${NC}"
    echo ""
    echo -e "${YELLOW}监控模块:${NC}"
    echo "  GPU  ·  CPU  ·  内存(带宽+容量)  ·  功耗(RAPL)"
    echo "  电池  ·  磁盘  ·  网络           ·  NPU"
else
    echo -e "${RED}[错误] 编译失败，未找到 ${PROJECT}${NC}"
    exit 1
fi