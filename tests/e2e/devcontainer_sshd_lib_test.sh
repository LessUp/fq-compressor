#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

# shellcheck source=/dev/null
source "${PROJECT_ROOT}/.devcontainer/scripts/lib/sshd.sh"

TEST_ROOT="$(mktemp -d)"
SETUP_SCRIPT="${TEST_ROOT}/setup-sshd.sh"
SENTINEL="${TEST_ROOT}/.sshd-setup-done"

cleanup() {
    rm -rf "${TEST_ROOT}"
}
trap cleanup EXIT

printf '#!/usr/bin/env bash\n' > "${SETUP_SCRIPT}"

FQCOMPRESSOR_SSH_PORT=2299
export FQCOMPRESSOR_SSH_PORT

[[ "$(sshd_port)" == "2299" ]]
[[ "$(sshd_setup_script_path "/workspace")" == "/workspace/.devcontainer/scripts/setup-sshd.sh" ]]
[[ "$(sshd_setup_sentinel_path)" == "/tmp/.sshd-setup-done" ]]

config="$(sshd_render_config "developer" "/home/developer/.ssh_authorized_keys")"
grep -Fq "Port 2299" <<<"${config}"
grep -Fq "AuthorizedKeysFile /home/developer/.ssh_authorized_keys" <<<"${config}"
grep -Fq "AllowUsers developer" <<<"${config}"

sshd_setup_needed "${SETUP_SCRIPT}" "${SENTINEL}"
touch "${SENTINEL}"
if sshd_setup_needed "${SETUP_SCRIPT}" "${SENTINEL}"; then
    echo "Expected sshd setup to be skipped when the sentinel is fresh." >&2
    exit 1
fi
sleep 1
touch "${SETUP_SCRIPT}"
sshd_setup_needed "${SETUP_SCRIPT}" "${SENTINEL}"
