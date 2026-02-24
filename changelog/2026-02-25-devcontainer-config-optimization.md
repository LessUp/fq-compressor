# DevContainer 配置优化

**日期**: 2026-02-25

## 变更内容

### 删除冗余配置
- 删除 `devcontainer.simple.json`，统一使用 docker-compose 模式（代理参数已在 `docker-compose.yml` 中支持）

### 文件组织优化
- 将 `setup-sshd.sh` 和 `start-sshd.sh` 从 `.devcontainer/` 根目录移至 `.devcontainer/scripts/`，与其他脚本统一管理
- 更新所有引用路径：`container-setup.sh`、`start-sshd.sh`、`docker/start_devcontainer.sh`

### 配置修复
- 移除 clangd 废弃参数 `--suggest-missing-includes`（clangd 15+ 已移除该选项）
- 修复端口转发重复：docker-compose 已映射 2222 端口，从 `forwardPorts` 中移除避免双重转发

### 代码质量
- 合并 `container-setup.sh` 中 `cmd_start()` / `cmd_attach()` 的重复逻辑，提取为 `_sync_and_start()` 函数

### Bug 修复（从 fastq-tools 反向移植）
- 修复 `setup-sshd.sh` 中 `collect_keys()` 返回值 bug：`return $keys_found` 在找到密钥时返回 1（bash 中表示失败），导致逻辑反转
- 修复 `container-setup.sh` 中 SSH agent 检查：仅在 `/ssh-agent` 挂载点存在但不是有效 socket 时才警告

### 健壮性提升（从 fastq-tools 反向移植）
- `start-sshd.sh` 添加哨兵文件 `/tmp/.sshd-setup-done`，避免每次启动都重新运行 setup-sshd.sh
- `host-prepare.sh` 添加 `ensure_dir ~/.ssh` + `chmod 700`，防止 bind mount 失败

### 文档更新
- 更新 `README.md`：移除 simple.json 相关说明、修复文件树中 SSHD 脚本重复列出的问题、更新脚本路径

## 影响范围
- `.devcontainer/devcontainer.json`
- `.devcontainer/devcontainer.simple.json`（已删除）
- `.devcontainer/scripts/setup-sshd.sh`（从根目录移入 + bug 修复）
- `.devcontainer/scripts/start-sshd.sh`（从根目录移入 + 哨兵优化）
- `.devcontainer/scripts/container-setup.sh`
- `.devcontainer/scripts/host-prepare.sh`
- `.devcontainer/README.md`
- `docker/start_devcontainer.sh`
