#!/usr/bin/env bash
# =============================================================================
# FQCompressor DevContainer - SSHD 配置脚本
# =============================================================================
# 配置 sshd 以支持远程 IDE 连接（Windsurf/Cursor/VS Code Remote-SSH）
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
    log_warn "sshd 未安装，跳过配置"
    exit 0
fi

# 配置常量
SSHD_PORT="${FQCOMPRESSOR_SSH_PORT:-2222}"
DEVELOPER_USER="developer"
AUTHORIZED_KEYS_FILE="/home/${DEVELOPER_USER}/.ssh_authorized_keys"
CONF_MAIN="/etc/ssh/sshd_config"
CONF_D="/etc/ssh/sshd_config.d"
CONF_SNIPPET="${CONF_D}/fqcompressor-devcontainer.conf"

log_info "配置 SSHD (端口: ${SSHD_PORT})..."

# 创建运行目录和生成主机密钥
sudo mkdir -p /var/run/sshd
sudo ssh-keygen -A 2>/dev/null || true

# 生成 sshd 配置
generate_sshd_config() {
    cat <<EOF
Port ${SSHD_PORT}
PasswordAuthentication no
KbdInteractiveAuthentication no
PubkeyAuthentication yes
PermitRootLogin no
AuthorizedKeysFile ${AUTHORIZED_KEYS_FILE}
AllowUsers ${DEVELOPER_USER}
EOF
}

# 应用配置
if sudo test -d "${CONF_D}" && sudo grep -Fq "Include /etc/ssh/sshd_config.d/*.conf" "${CONF_MAIN}" 2>/dev/null; then
    # 使用 sshd_config.d 目录
    generate_sshd_config | sudo tee "${CONF_SNIPPET}" >/dev/null
    log_info "配置写入 ${CONF_SNIPPET}"
else
    # 直接修改主配置文件
    MARK_START="# FQCOMPRESSOR_DEVCONTAINER_SSHD_CONFIG_START"
    MARK_END="# FQCOMPRESSOR_DEVCONTAINER_SSHD_CONFIG_END"

    # 删除旧配置
    sudo sed -i "/^${MARK_START}$/,/^${MARK_END}$/d" "${CONF_MAIN}" 2>/dev/null || true

    # 追加新配置
    {
        echo "${MARK_START}"
        generate_sshd_config
        echo "${MARK_END}"
    } | sudo tee -a "${CONF_MAIN}" >/dev/null

    log_info "配置追加到 ${CONF_MAIN}"
fi

# 收集 authorized_keys
TMP_KEYS="$(mktemp)"
trap 'rm -f "${TMP_KEYS}"' EXIT

collect_keys() {
    local keys_found=0

    # 优先使用 authorized_keys
    if [ -s "/home/${DEVELOPER_USER}/.ssh/authorized_keys" ]; then
        cat "/home/${DEVELOPER_USER}/.ssh/authorized_keys" >> "${TMP_KEYS}"
        keys_found=1
    fi

    # 备选：使用公钥文件
    if [ "$keys_found" -eq 0 ]; then
        for pubkey in id_ed25519.pub id_rsa.pub id_ecdsa.pub id_dsa.pub; do
            local keyfile="/home/${DEVELOPER_USER}/.ssh/${pubkey}"
            if [ -s "$keyfile" ]; then
                cat "$keyfile" >> "${TMP_KEYS}"
                echo "" >> "${TMP_KEYS}"
                keys_found=1
            fi
        done
    fi

    # bash: return 0 = 成功, return 1 = 失败
    if [ "$keys_found" -eq 1 ]; then
        return 0
    else
        return 1
    fi
}

if collect_keys; then
    log_info "已收集 SSH 公钥"
else
    log_warn "未找到 SSH 公钥，SSHD 可能无法进行公钥认证"
    touch "${TMP_KEYS}"
fi

# 安装 authorized_keys
sudo install -o "${DEVELOPER_USER}" -g "${DEVELOPER_USER}" -m 0600 "${TMP_KEYS}" "${AUTHORIZED_KEYS_FILE}"

log_info "SSHD 配置完成"
