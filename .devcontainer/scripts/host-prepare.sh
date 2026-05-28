#!/usr/bin/env bash
# =============================================================================
# FQCompressor DevContainer - 宿主机准备脚本
# =============================================================================
# 在 devcontainer 启动前运行（initializeCommand），准备宿主机上的文件和目录
#
# 支持平台:
#   - WSL2 (推荐 Windows 开发路径)
#   - 原生 Linux / 远程服务器
#   - Windows Git Bash (有限支持)
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=/dev/null
source "${SCRIPT_DIR}/lib/host_sync.sh"

# 颜色输出
RED='\033[0;31m'
YELLOW='\033[1;33m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
NC='\033[0m'

log_info()  { echo -e "${GREEN}[INFO]${NC} $*"; }
log_warn()  { echo -e "${YELLOW}[WARN]${NC} $*" >&2; }
log_error() { echo -e "${RED}[ERROR]${NC} $*" >&2; }

# =============================================================================
# 平台检测
# =============================================================================
# 返回: "wsl" | "linux" | "windows" | "unknown"
detect_platform() {
    if [ -n "${WSL_DISTRO_NAME:-}" ] || grep -qis "microsoft" /proc/version 2>/dev/null; then
        echo "wsl"
    elif [ "$(uname -s 2>/dev/null)" = "Linux" ]; then
        echo "linux"
    elif [[ "$(uname -s 2>/dev/null)" == MINGW* ]] || [ -n "${MSYSTEM:-}" ]; then
        echo "windows"
    else
        echo "unknown"
    fi
}

# =============================================================================
# 工具函数
# =============================================================================

# 获取 HOME 目录
get_home() {
    local h="${HOME:-}"
    if [ -z "$h" ] && command -v getent >/dev/null 2>&1; then
        h="$(getent passwd "$(id -un)" | cut -d: -f6)"
    fi
    echo "$h"
}

main() {
    local PLATFORM
    PLATFORM="$(detect_platform)"

    local H
    H="$(get_home)"

    if [ -z "$H" ]; then
        log_error "无法确定 HOME 目录"
        exit 1
    fi

    log_info "准备 devcontainer 宿主机文件..."
    log_info "检测平台: ${CYAN}${PLATFORM}${NC}"

    # -----------------------------------------------------------------
    # 平台特定提示
    # -----------------------------------------------------------------
    case "$PLATFORM" in
        wsl)
            log_info "WSL2 环境 (${WSL_DISTRO_NAME:-unknown}) — 推荐的 Windows 开发路径"
            ;;
        linux)
            log_info "原生 Linux 环境"
            ;;
        windows)
            log_warn "检测到 Windows 原生环境 (Git Bash / MSYS2)"
            log_warn "强烈建议改用 WSL2："
            log_warn "  1. 在 WSL 中打开项目: code --remote wsl+<distro> /path/to/fq-compressor"
            log_warn "  2. 然后执行 Reopen in Container"
            log_warn "Windows 原生的 volume 性能极差，且部分功能受限"
            ;;
    esac

    local REPO_ROOT
    REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
    host_sync_stage_home "$H" "$H" "$REPO_ROOT"

    # 5. 检查 SSH agent（提示用户）
    if [ -n "${SSH_AUTH_SOCK:-}" ] && [ -S "${SSH_AUTH_SOCK}" ]; then
        log_info "SSH agent 已检测到: ${SSH_AUTH_SOCK}"
    else
        log_warn "SSH agent 未运行。如需 Git SSH 认证，请先执行："
        log_warn "  eval \"\$(ssh-agent -s)\" && ssh-add"
    fi

    # 6. 平台特定检查
    if [ "$PLATFORM" = "wsl" ]; then
        # 检查项目是否在 WSL 文件系统中（/mnt/c 路径性能差）
        local cwd
        cwd="$(pwd)"
        if [[ "$cwd" == /mnt/* ]]; then
            log_warn "项目位于 Windows 挂载路径 ($cwd)"
            log_warn "volume 性能较差，建议将项目移到 WSL 原生文件系统 (~/ 或 /home/)"
        fi
    fi

    log_info "宿主机文件准备完成"
}

main "$@"
