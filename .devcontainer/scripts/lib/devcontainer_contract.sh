#!/usr/bin/env bash

# Shared devcontainer contract for IDE adapters, runtime helpers, and validation.

readonly FQC_DEVCONTAINER_WORKSPACE="/workspace"
readonly FQC_DEVCONTAINER_REMOTE_USER="developer"
readonly FQC_DEVCONTAINER_SERVICE="dev"
readonly FQC_DEVCONTAINER_COMPOSE_FILE="../docker/docker-compose.yml"
readonly FQC_DEVCONTAINER_INIT_COMMAND='bash ${localWorkspaceFolder}/.devcontainer/scripts/host-prepare.sh'
readonly FQC_DEVCONTAINER_SETUP_SCRIPT="${FQC_DEVCONTAINER_WORKSPACE}/.devcontainer/scripts/container-setup.sh"
readonly FQC_DEVCONTAINER_POST_CREATE="bash ${FQC_DEVCONTAINER_SETUP_SCRIPT} create"
readonly FQC_DEVCONTAINER_POST_START="bash ${FQC_DEVCONTAINER_SETUP_SCRIPT} start"
readonly FQC_DEVCONTAINER_POST_ATTACH="bash ${FQC_DEVCONTAINER_SETUP_SCRIPT} attach"
readonly FQC_DEVCONTAINER_CLION_POST_CREATE="${FQC_DEVCONTAINER_POST_CREATE} || true"
readonly FQC_DEVCONTAINER_CLION_POST_START="${FQC_DEVCONTAINER_POST_START} || true"
readonly FQC_DEVCONTAINER_CLION_POST_ATTACH="${FQC_DEVCONTAINER_POST_ATTACH} || true"
readonly FQC_DEVCONTAINER_CLION_SSH_PORT_MAP="127.0.0.1:2222:2222"
readonly FQC_DEVCONTAINER_CLION_CONAN_VOLUME="fqcompressor_conan_cache:/home/developer/.conan2"
readonly FQC_DEVCONTAINER_CLION_CCACHE_VOLUME="fqcompressor_ccache_cache:/home/developer/.ccache"

readonly FQC_DEVCONTAINER_GITCONFIG_MOUNT="type=bind,source=\${localEnv:HOME}/.fqcompressor-host-gitconfig,target=/tmp/host-gitconfig,readonly"
readonly FQC_DEVCONTAINER_CLAUDE_MOUNT="type=bind,source=\${localEnv:HOME}/.fqcompressor-host-claude,target=/tmp/host-claude,readonly"
readonly FQC_DEVCONTAINER_CODEX_MOUNT="type=bind,source=\${localEnv:HOME}/.fqcompressor-host-codex,target=/tmp/host-codex,readonly"

devcontainer_required_files() {
    cat <<'EOF'
.devcontainer/devcontainer.json
.devcontainer/devcontainer.clion.json
.devcontainer/scripts/lib/devcontainer_contract.sh
.devcontainer/scripts/lib/host_sync.sh
.devcontainer/scripts/lib/sshd.sh
docker/docker-compose.yml
EOF
}

devcontainer_required_scripts() {
    cat <<'EOF'
.devcontainer/scripts/host-prepare.sh
.devcontainer/scripts/container-setup.sh
EOF
}

devcontainer_required_executables() {
    cat <<'EOF'
.devcontainer/scripts/setup-sshd.sh
.devcontainer/scripts/start-sshd.sh
EOF
}
