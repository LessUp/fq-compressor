#!/usr/bin/env bash
set -euo pipefail

if ! command -v sshd >/dev/null 2>&1; then
  exit 0
fi

AUTHORIZED_KEYS_FALLBACK="/home/developer/.ssh_authorized_keys"

CONF_MAIN="/etc/ssh/sshd_config"
CONF_D="/etc/ssh/sshd_config.d"
CONF_SNIPPET="${CONF_D}/fqcompressor-devcontainer.conf"

sudo mkdir -p /var/run/sshd
sudo ssh-keygen -A

if sudo test -d "${CONF_D}" && sudo grep -Fq "Include /etc/ssh/sshd_config.d/*.conf" "${CONF_MAIN}"; then
  sudo tee "${CONF_SNIPPET}" >/dev/null <<'EOF'
Port 2222
PasswordAuthentication no
KbdInteractiveAuthentication no
PubkeyAuthentication yes
PermitRootLogin no
AuthorizedKeysFile /home/developer/.ssh_authorized_keys
AllowUsers developer
EOF
else
  MARK_START="# FQCOMPRESSOR_DEVCONTAINER_SSHD_CONFIG_START"
  MARK_END="# FQCOMPRESSOR_DEVCONTAINER_SSHD_CONFIG_END"

  sudo sed -i "/^${MARK_START}$/,/^${MARK_END}$/d" "${CONF_MAIN}"

  sudo tee -a "${CONF_MAIN}" >/dev/null <<EOF
${MARK_START}
Port 2222
PasswordAuthentication no
KbdInteractiveAuthentication no
PubkeyAuthentication yes
PermitRootLogin no
AuthorizedKeysFile /home/developer/.ssh_authorized_keys
AllowUsers developer
${MARK_END}
EOF
fi

TMP_KEYS="$(mktemp)"
trap 'rm -f "${TMP_KEYS}"' EXIT

if test -s /home/developer/.ssh/authorized_keys; then
  cat /home/developer/.ssh/authorized_keys >"${TMP_KEYS}"
else
  for candidate in /home/developer/.ssh/id_ed25519.pub /home/developer/.ssh/id_rsa.pub /home/developer/.ssh/id_ecdsa.pub /home/developer/.ssh/id_dsa.pub; do
    if test -s "${candidate}"; then
      cat "${candidate}" >>"${TMP_KEYS}"
      printf '\n' >>"${TMP_KEYS}"
    fi
  done
fi

if ! test -s "${TMP_KEYS}"; then
  : >"${TMP_KEYS}"
fi

sudo install -o developer -g developer -m 0600 "${TMP_KEYS}" "${AUTHORIZED_KEYS_FALLBACK}"
