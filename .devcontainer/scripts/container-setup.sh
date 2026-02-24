#!/usr/bin/env bash
# =============================================================================
# FQCompressor DevContainer - 容器内设置脚本
# =============================================================================
# 在容器内运行，配置开发环境
# 用于 postCreateCommand / postStartCommand / postAttachCommand
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

# 配置常量
DEVELOPER_HOME="/home/developer"
WORKSPACE="${WORKSPACE:-/workspace}"

# =============================================================================
# 配置同步函数
# =============================================================================

# 同步 gitconfig
sync_gitconfig() {
    local target="$DEVELOPER_HOME/.gitconfig"
    local source="/tmp/host-gitconfig"

    if [ -d "$target" ]; then
        rm -rf "$target"
    fi

    if [ ! -e "$target" ]; then
        touch "$target"
    fi

    if [ -f "$source" ]; then
        cp -f "$source" "$target"
    fi
}

# 同步 Claude 配置
sync_claude_config() {
    local target_dir="$DEVELOPER_HOME/.claude"
    local source_dir="/tmp/host-claude"

    mkdir -p "$target_dir"

    local files=(
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

    for f in "${files[@]}"; do
        if [ -s "$source_dir/$f" ]; then
            cp -f "$source_dir/$f" "$target_dir/$f"
        fi
    done

    # Claude CLI 登录信息（宿主机侧已放入 /tmp/host-claude/.claude.json）
    if [ -s "$source_dir/.claude.json" ]; then
        cp -f "$source_dir/.claude.json" "$DEVELOPER_HOME/.claude.json"
    fi
}

# 同步 Codex 配置
sync_codex_config() {
    local target_dir="$DEVELOPER_HOME/.codex"
    local source_dir="/tmp/host-codex"

    mkdir -p "$target_dir"

    local files=("auth.json" "config.toml")

    for f in "${files[@]}"; do
        if [ -s "$source_dir/$f" ]; then
            cp -f "$source_dir/$f" "$target_dir/$f"
        fi
    done
}

# 配置 Git
setup_git() {
    if ! command -v git >/dev/null 2>&1; then
        return
    fi

    if ! git config --global --get-all safe.directory 2>/dev/null | grep -Fxq "$WORKSPACE"; then
        git config --global --add safe.directory "$WORKSPACE"
    fi

    if [ -d "$WORKSPACE/.git" ] && [ -f "$WORKSPACE/.gitmessage.txt" ]; then
        git config commit.template "$WORKSPACE/.gitmessage.txt" 2>/dev/null || true
    fi
}

# 初始化 Conan
setup_conan() {
    if command -v conan >/dev/null 2>&1; then
        conan profile detect --force >/dev/null 2>&1 || true
    fi
}

# 启动 SSHD
start_sshd() {
    local sshd_script="$WORKSPACE/.devcontainer/scripts/start-sshd.sh"
    if [ -x "$sshd_script" ]; then
        bash "$sshd_script" || true
    fi
}

# 检查环境警告
check_environment() {
    if [ -z "${WSL_DISTRO_NAME:-}" ]; then
        log_warn "建议使用 VS Code Remote - WSL 在 WSL2 中打开仓库，再执行 Reopen in Container"
    fi

    # 仅当 /ssh-agent 挂载点存在但不是有效 socket 时才警告
    if [ -e "/ssh-agent" ] && [ ! -S "/ssh-agent" ]; then
        log_warn "SSH agent socket 存在但不可用。若需要访问私有仓库，请在 WSL 中执行："
        log_warn "  eval \"\$(ssh-agent -s)\" && ssh-add"
        log_warn "然后 Rebuild/Reopen Container"
    fi
}

# =============================================================================
# 主入口
# =============================================================================

usage() {
    cat <<EOF
用法: $0 <command>

命令:
  create    postCreateCommand - 首次创建容器时运行
  start     postStartCommand  - 每次启动容器时运行
  attach    postAttachCommand - 每次附加到容器时运行
  sync      仅同步配置文件
EOF
}

cmd_create() {
    log_info "执行 postCreateCommand..."

    cd "$WORKSPACE" || exit 0

    check_environment
    sync_gitconfig
    sync_claude_config
    sync_codex_config
    setup_git
    setup_conan

    local setup_script="$WORKSPACE/.devcontainer/scripts/setup-sshd.sh"
    if [ -x "$setup_script" ]; then
        bash "$setup_script" || true
    fi

    log_info "postCreateCommand 完成"
}

_sync_and_start() {
    sync_gitconfig
    sync_claude_config
    sync_codex_config
    setup_git
    start_sshd
}

cmd_start() {
    log_info "执行 postStartCommand..."
    _sync_and_start
    log_info "postStartCommand 完成"
}

cmd_attach() {
    log_info "执行 postAttachCommand..."
    _sync_and_start
    log_info "postAttachCommand 完成"
}

cmd_sync() {
    log_info "同步配置文件..."

    sync_gitconfig
    sync_claude_config
    sync_codex_config

    log_info "配置同步完成"
}

main() {
    local cmd="${1:-}"

    case "$cmd" in
        create)  cmd_create ;;
        start)   cmd_start ;;
        attach)  cmd_attach ;;
        sync)    cmd_sync ;;
        -h|--help|help) usage ;;
        *)
            log_error "未知命令: $cmd"
            usage
            exit 1
            ;;
    esac
}

main "$@"
