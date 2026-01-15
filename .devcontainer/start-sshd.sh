#!/usr/bin/env bash
set -euo pipefail

if ! command -v sshd >/dev/null 2>&1; then
  exit 0
fi

bash /usr/local/bin/setup-sshd.sh || true

if pgrep -x sshd >/dev/null 2>&1; then
  exit 0
fi

sudo /usr/sbin/sshd
