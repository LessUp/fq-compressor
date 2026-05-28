#!/usr/bin/env bash

sshd_port() {
    printf '%s\n' "${FQCOMPRESSOR_SSH_PORT:-2222}"
}

sshd_authorized_keys_file() {
    local developer_user="${1:-developer}"
    printf '/home/%s/.ssh_authorized_keys\n' "${developer_user}"
}

sshd_setup_script_path() {
    local workspace="${1:-/workspace}"
    printf '%s/.devcontainer/scripts/setup-sshd.sh\n' "${workspace}"
}

sshd_setup_sentinel_path() {
    printf '/tmp/.sshd-setup-done\n'
}

sshd_render_config() {
    local developer_user="${1:-developer}"
    local authorized_keys_file="${2:-$(sshd_authorized_keys_file "$developer_user")}"

    cat <<EOF
Port $(sshd_port)
PasswordAuthentication no
KbdInteractiveAuthentication no
PubkeyAuthentication yes
PermitRootLogin no
AuthorizedKeysFile ${authorized_keys_file}
AllowUsers ${developer_user}
EOF
}

sshd_setup_needed() {
    local setup_script="$1"
    local sentinel="$2"

    [[ ! -f "${sentinel}" || "${setup_script}" -nt "${sentinel}" ]]
}
