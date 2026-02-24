#!/usr/bin/env bash
# =============================================================================
# FQCompressor DevContainer - SSHD 启动脚本
# =============================================================================
# 启动 sshd 服务（如果尚未运行）
# =============================================================================
set -euo pipefail

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
SETUP_SCRIPT="${WORKSPACE}/.devcontainer/scripts/setup-sshd.sh"
SETUP_SENTINEL="/tmp/.sshd-setup-done"

# 仅在首次或脚本更新后重新配置
if [ -x "$SETUP_SCRIPT" ]; then
    if [ ! -f "$SETUP_SENTINEL" ] || [ "$SETUP_SCRIPT" -nt "$SETUP_SENTINEL" ]; then
        bash "$SETUP_SCRIPT" || true
        touch "$SETUP_SENTINEL"
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
