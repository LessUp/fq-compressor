#!/usr/bin/env bash
# =============================================================================
# FQCompressor DevContainer - 手动启动脚本
# =============================================================================
# 在远程服务器上手动启动 dev 容器，用于 Windsurf/Cursor/Remote-SSH 连接
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
log_step()  { echo -e "${CYAN}[STEP]${NC} $*"; }

# =============================================================================
# Docker Compose 兼容层
# =============================================================================
compose() {
    if docker compose version >/dev/null 2>&1; then
        docker compose "$@"
    elif command -v docker-compose >/dev/null 2>&1; then
        docker-compose "$@"
    else
        log_error "未找到 docker compose 或 docker-compose"
        exit 127
    fi
}

# =============================================================================
# 使用说明
# =============================================================================
usage() {
    cat <<EOF
${CYAN}FQCompressor DevContainer 启动脚本${NC}

用法:
  $0 [选项]

选项:
  --bind <ip>               宿主机端口绑定地址 (默认: 127.0.0.1)
                            异机访问可用 0.0.0.0
  --port <port>             宿主机 SSH 端口 (默认: 2222)
  --authorized-keys <file>  authorized_keys 文件路径
                            (默认: \$HOME/.ssh/authorized_keys)
  --no-build                跳过 docker compose build
  --rebuild                 强制重新构建镜像
  -h, --help                显示此帮助信息

示例:
  # 默认启动（仅本地访问）
  $0

  # 允许远程访问
  $0 --bind 0.0.0.0

  # 使用自定义端口和密钥
  $0 --port 2223 --authorized-keys ~/.ssh/my_keys

环境变量:
  FQCOMPRESSOR_SSH_BIND       等同于 --bind
  FQCOMPRESSOR_SSH_PORT       等同于 --port
  FQCOMPRESSOR_AUTHORIZED_KEYS_FILE  等同于 --authorized-keys
EOF
}

# =============================================================================
# 参数解析
# =============================================================================
BIND="${FQCOMPRESSOR_SSH_BIND:-127.0.0.1}"
PORT="${FQCOMPRESSOR_SSH_PORT:-2222}"
AUTHORIZED_KEYS_FILE="${FQCOMPRESSOR_AUTHORIZED_KEYS_FILE:-${HOME}/.ssh/authorized_keys}"
NO_BUILD=0
REBUILD=0

while [ $# -gt 0 ]; do
    case "$1" in
        --bind)
            BIND="$2"; shift 2 ;;
        --port)
            PORT="$2"; shift 2 ;;
        --authorized-keys)
            AUTHORIZED_KEYS_FILE="$2"; shift 2 ;;
        --no-build)
            NO_BUILD=1; shift ;;
        --rebuild)
            REBUILD=1; shift ;;
        -h|--help)
            usage; exit 0 ;;
        *)
            log_error "未知参数: $1"
            usage
            exit 2 ;;
    esac
done

# =============================================================================
# 路径设置
# =============================================================================
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
COMPOSE_FILE="${REPO_ROOT}/docker/docker-compose.yml"

# 导出环境变量供 docker-compose 使用
export FQCOMPRESSOR_SSH_BIND="${BIND}"
export FQCOMPRESSOR_SSH_PORT="${PORT}"

# =============================================================================
# 主流程
# =============================================================================
main() {
    log_info "FQCompressor DevContainer 启动"
    log_info "SSH 绑定: ${BIND}:${PORT}"

    # 1. 构建镜像
    if [ "${REBUILD}" -eq 1 ]; then
        log_step "强制重新构建镜像..."
        compose -f "${COMPOSE_FILE}" build --no-cache dev
    elif [ "${NO_BUILD}" -eq 0 ]; then
        log_step "构建镜像（如有更新）..."
        compose -f "${COMPOSE_FILE}" build dev
    else
        log_step "跳过构建"
    fi

    # 2. 启动容器
    log_step "启动容器..."
    compose -f "${COMPOSE_FILE}" up -d dev

    # 3. 注入 authorized_keys
    log_step "配置 SSH 认证..."
    if [ -f "${AUTHORIZED_KEYS_FILE}" ]; then
        compose -f "${COMPOSE_FILE}" exec -T -u developer dev bash -lc \
            'mkdir -p ~/.ssh && chmod 700 ~/.ssh'
        compose -f "${COMPOSE_FILE}" exec -T -u developer dev bash -lc \
            'cat > ~/.ssh/authorized_keys && chmod 600 ~/.ssh/authorized_keys' \
            < "${AUTHORIZED_KEYS_FILE}"
        log_info "已注入 authorized_keys: ${AUTHORIZED_KEYS_FILE}"
    else
        log_warn "未找到 authorized_keys: ${AUTHORIZED_KEYS_FILE}"
        log_warn "SSHD 将启动，但可能无法进行公钥认证"
    fi

    # 4. 启动 SSHD
    log_step "启动 SSHD..."
    compose -f "${COMPOSE_FILE}" exec -T -u developer dev bash -lc \
        'bash /workspace/.devcontainer/scripts/start-sshd.sh'

    # 5. 完成
    echo ""
    log_info "${GREEN}DevContainer 启动成功！${NC}"
    echo ""
    echo -e "  ${CYAN}SSH 连接:${NC}"
    echo -e "    ssh -p ${PORT} developer@<服务器IP>"
    echo ""
    echo -e "  ${CYAN}VS Code Remote-SSH 配置:${NC}"
    echo -e "    Host fqcompressor-dev"
    echo -e "        HostName <服务器IP>"
    echo -e "        Port ${PORT}"
    echo -e "        User developer"
    echo ""

    if [ "${BIND}" = "127.0.0.1" ]; then
        log_warn "当前绑定到 127.0.0.1，仅本机可访问"
        log_warn "如需远程访问，请使用: $0 --bind 0.0.0.0"
    fi
}

main "$@"
