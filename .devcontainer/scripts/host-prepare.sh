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

# 确保路径是文件（不是目录）
ensure_file() {
    local path="$1"
    local default_content="${2:-}"

    if [ -d "$path" ]; then
        log_warn "$path 是目录，正在删除并创建为文件..."
        rm -rf "$path"
    fi

    if [ ! -e "$path" ]; then
        if [ -n "$default_content" ] && [ -f "$default_content" ]; then
            cp -f "$default_content" "$path"
        else
            touch "$path"
        fi
    fi
}

# 确保路径是目录（不是文件）
ensure_dir() {
    local path="$1"

    if [ -e "$path" ] && [ ! -d "$path" ]; then
        log_warn "$path 是文件，正在删除并创建为目录..."
        rm -f "$path"
    fi

    mkdir -p "$path"
}

# 同步文件到目标目录
sync_files() {
    local src_dir="$1"
    local dst_dir="$2"
    shift 2
    local files=("$@")

    ensure_dir "$dst_dir"

    for f in "${files[@]}"; do
        local src="$src_dir/$f"
        local dst="$dst_dir/$f"

        if [ -d "$dst" ]; then
            rm -rf "$dst"
        fi

        if [ -f "$src" ]; then
            cp -f "$src" "$dst"
        elif [ ! -e "$dst" ]; then
            touch "$dst"
        fi
    done
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

    # 0. 确保 ~/.ssh 目录存在（devcontainer 需要 bind mount）
    ensure_dir "$H/.ssh"
    chmod 700 "$H/.ssh" 2>/dev/null || true

    # 1. 准备 gitconfig
    ensure_file "$H/.gitconfig"
    ensure_file "$H/.fqcompressor-host-gitconfig" "$H/.gitconfig"
    cp -f "$H/.gitconfig" "$H/.fqcompressor-host-gitconfig"

    # 2. 准备 Claude 配置
    local claude_files=(
        "CLAUDE.md"
        "config.json"
        "settings.json"
        "settings.duck.json"
        "settings.glm.json"
        "settings.kimi.json"
        "settings.local.json"
        "settings.minimax.json"
        "settings.wong.json"
    )
    sync_files "$H/.claude" "$H/.fqcompressor-host-claude" "${claude_files[@]}"

    # Claude CLI 登录信息（位于 HOME 根目录）
    local claude_root_src="$H/.claude.json"
    local claude_root_dst="$H/.fqcompressor-host-claude/.claude.json"
    if [ -f "$claude_root_src" ]; then
        cp -f "$claude_root_src" "$claude_root_dst"
    elif [ ! -e "$claude_root_dst" ]; then
        touch "$claude_root_dst"
    fi

    # 3. 准备 Codex 配置
    local codex_files=(
        "auth.json"
        "config.toml"
    )
    sync_files "$H/.codex" "$H/.fqcompressor-host-codex" "${codex_files[@]}"

    # 4. 确保数据目录存在
    local SCRIPT_DIR
    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    local REPO_ROOT
    REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

    # 从 .env 读取数据路径（如果存在）
    local DATA_PATH="${FQCOMPRESSOR_HOST_DATA_DIR:-}"
    if [ -z "$DATA_PATH" ] && [ -f "$REPO_ROOT/docker/.env" ]; then
        DATA_PATH="$(grep -E '^FQCOMPRESSOR_HOST_DATA_DIR=' "$REPO_ROOT/docker/.env" 2>/dev/null | cut -d= -f2- || true)"
    fi
    DATA_PATH="${DATA_PATH:-/tmp/fqcompressor-data}"

    if [ ! -d "$DATA_PATH" ]; then
        log_info "创建数据目录: $DATA_PATH"
        mkdir -p "$DATA_PATH" 2>/dev/null || log_warn "无法创建数据目录: $DATA_PATH（可忽略）"
    fi

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
