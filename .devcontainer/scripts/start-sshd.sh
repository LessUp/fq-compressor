#!/usr/bin/env bash
# =============================================================================
# FQCompressor DevContainer - SSHD 启动脚本
# =============================================================================
# 启动 sshd 服务（如果尚未运行）
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=/dev/null
source "${SCRIPT_DIR}/lib/sshd.sh"

# 颜色输出
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info() { echo -e "${GREEN}[SSHD]${NC} $*"; }
log_warn() { echo -e "${YELLOW}[SSHD]${NC} $*" >&2; }

# 检查 sshd 是否可用
if ! command -v sshd >/dev/null 2>&1; then
    log_warn "sshd 未安装，跳过启动"
    exit 0
fi

WORKSPACE="${WORKSPACE:-/workspace}"
SETUP_SCRIPT="$(sshd_setup_script_path "${WORKSPACE}")"
SETUP_SENTINEL="$(sshd_setup_sentinel_path)"

# 仅在首次或脚本更新后重新配置
if [ -x "$SETUP_SCRIPT" ]; then
    if sshd_setup_needed "$SETUP_SCRIPT" "$SETUP_SENTINEL"; then
        if bash "$SETUP_SCRIPT"; then
            touch "$SETUP_SENTINEL"
        else
            log_warn "SSHD 配置失败，保留未完成状态以便下次重试"
            rm -f "$SETUP_SENTINEL"
        fi
    fi
fi

# 检查是否已运行
if pgrep -x sshd >/dev/null 2>&1; then
    log_info "SSHD 已在运行"
    exit 0
fi

# 启动 sshd
log_info "启动 SSHD..."
sudo /usr/sbin/sshd

# 验证启动
sleep 0.5
if pgrep -x sshd >/dev/null 2>&1; then
    log_info "SSHD 启动成功"
else
    log_warn "SSHD 启动可能失败，请检查日志"
fi
