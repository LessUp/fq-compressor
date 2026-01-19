#!/usr/bin/env bash
# =============================================================================
# FQCompressor DevContainer - 宿主机准备脚本
# =============================================================================
# 在 devcontainer 启动前运行，准备宿主机上的文件和目录
# 用于 initializeCommand
# =============================================================================
set -euo pipefail

# 颜色输出
RED='\033[0;31m'
YELLOW='\033[1;33m'
GREEN='\033[0;32m'
NC='\033[0m'

log_info()  { echo -e "${GREEN}[INFO]${NC} $*"; }
log_warn()  { echo -e "${YELLOW}[WARN]${NC} $*" >&2; }
log_error() { echo -e "${RED}[ERROR]${NC} $*" >&2; }

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
    local H
    H="$(get_home)"

    if [ -z "$H" ]; then
        log_error "无法确定 HOME 目录"
        exit 1
    fi

    log_info "准备 devcontainer 宿主机文件..."

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

    log_info "宿主机文件准备完成"
}

main "$@"
