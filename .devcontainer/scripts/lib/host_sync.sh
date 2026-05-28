#!/usr/bin/env bash

host_sync_claude_files() {
    cat <<'EOF'
CLAUDE.md
config.json
settings.json
settings.duck.json
settings.glm.json
settings.kimi.json
settings.local.json
settings.minimax.json
settings.wong.json
EOF
}

host_sync_codex_files() {
    cat <<'EOF'
auth.json
config.toml
EOF
}

host_sync_ensure_file() {
    local path="$1"
    local default_content="${2:-}"

    if [ -d "$path" ]; then
        rm -rf "$path"
    fi

    if [ ! -e "$path" ]; then
        if [ -n "$default_content" ] && [ -f "$default_content" ]; then
            cp -f "$default_content" "$path"
        else
            touch "$path"
        fi
    fi
}

host_sync_ensure_dir() {
    local path="$1"

    if [ -e "$path" ] && [ ! -d "$path" ]; then
        rm -f "$path"
    fi

    mkdir -p "$path"
}

host_sync_copy_stage_dir() {
    local src_dir="$1"
    local dst_dir="$2"
    local file_list_fn="$3"

    host_sync_ensure_dir "$dst_dir"

    while IFS= read -r relative; do
        local src="$src_dir/$relative"
        local dst="$dst_dir/$relative"

        if [ -d "$dst" ]; then
            rm -rf "$dst"
        fi

        if [ -f "$src" ]; then
            cp -f "$src" "$dst"
        elif [ ! -e "$dst" ]; then
            touch "$dst"
        fi
    done < <("$file_list_fn")
}

host_sync_copy_nonempty_dir() {
    local src_dir="$1"
    local dst_dir="$2"
    local file_list_fn="$3"

    host_sync_ensure_dir "$dst_dir"

    while IFS= read -r relative; do
        local src="$src_dir/$relative"
        if [ -s "$src" ]; then
            cp -f "$src" "$dst_dir/$relative"
        fi
    done < <("$file_list_fn")
}

host_sync_data_path() {
    local repo_root="$1"
    local data_path="${FQCOMPRESSOR_HOST_DATA_DIR:-}"

    if [ -z "$data_path" ] && [ -f "$repo_root/docker/.env" ]; then
        data_path="$(grep -E '^FQCOMPRESSOR_HOST_DATA_DIR=' "$repo_root/docker/.env" 2>/dev/null | cut -d= -f2- || true)"
    fi

    printf '%s\n' "${data_path:-/tmp/fqcompressor-data}"
}

host_sync_stage_home() {
    local host_home="$1"
    local staging_root="$2"
    local repo_root="$3"

    host_sync_ensure_dir "$host_home/.ssh"
    chmod 700 "$host_home/.ssh" 2>/dev/null || true

    host_sync_ensure_dir "$staging_root"

    host_sync_ensure_file "$host_home/.gitconfig"
    host_sync_ensure_file "$staging_root/.fqcompressor-host-gitconfig" "$host_home/.gitconfig"
    cp -f "$host_home/.gitconfig" "$staging_root/.fqcompressor-host-gitconfig"

    host_sync_copy_stage_dir "$host_home/.claude" "$staging_root/.fqcompressor-host-claude" host_sync_claude_files
    if [ -f "$host_home/.claude.json" ]; then
        cp -f "$host_home/.claude.json" "$staging_root/.fqcompressor-host-claude/.claude.json"
    elif [ ! -e "$staging_root/.fqcompressor-host-claude/.claude.json" ]; then
        touch "$staging_root/.fqcompressor-host-claude/.claude.json"
    fi

    host_sync_copy_stage_dir "$host_home/.codex" "$staging_root/.fqcompressor-host-codex" host_sync_codex_files

    local data_path
    data_path="$(host_sync_data_path "$repo_root")"
    if [ ! -d "$data_path" ]; then
        mkdir -p "$data_path" 2>/dev/null || true
    fi
}

host_sync_apply_container_home() {
    local container_home="$1"
    local staging_root="$2"

    local git_target="$container_home/.gitconfig"
    local git_source="$staging_root/.fqcompressor-host-gitconfig"
    if [ -d "$git_target" ]; then
        rm -rf "$git_target"
    fi
    if [ ! -e "$git_target" ]; then
        touch "$git_target"
    fi
    if [ -f "$git_source" ]; then
        cp -f "$git_source" "$git_target"
    fi

    host_sync_copy_nonempty_dir "$staging_root/.fqcompressor-host-claude" "$container_home/.claude" host_sync_claude_files
    if [ -s "$staging_root/.fqcompressor-host-claude/.claude.json" ]; then
        cp -f "$staging_root/.fqcompressor-host-claude/.claude.json" "$container_home/.claude.json"
    fi

    host_sync_copy_nonempty_dir "$staging_root/.fqcompressor-host-codex" "$container_home/.codex" host_sync_codex_files
}
