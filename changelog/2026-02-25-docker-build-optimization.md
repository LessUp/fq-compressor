# Docker 打包部署优化

**日期**: 2026-02-25

## 变更内容

### 生产 Dockerfile 构建阶段精简
- 移除构建阶段中不需要的开发工具：vim, neovim, less, tree, grep, htop, tmux, ripgrep, zip, unzip, jq, tar, xz-utils, gnupg, lsb-release, software-properties-common
- 仅保留编译、下载和包管理所需的最小依赖，减少构建时间和层缓存大小

### 配置一致性修复
- 统一 `Dockerfile` 中 `USE_CHINA_MIRROR` 默认值为 `0`，与 `Dockerfile.dev` 和 `.env.example` 保持一致

### 代理环境变量规范化
- 为 `Dockerfile` 中的代理 ARG 添加默认空值，与 `Dockerfile.dev` 保持一致
- 添加注释说明代理仅在 build 阶段生效，不会泄漏到 production 镜像

### 清理无用指令
- 移除 `Dockerfile.dev` 中无用的 `RUN touch /tmp/host-gitconfig`（该文件会被 devcontainer bind mount 覆盖）

### 健康检查改进
- `docker-compose.yml` dev 服务 healthcheck 从 `pgrep -x bash`（恒为 true）改为 `gcc --version`（验证编译器可用性）
- 检查间隔从 30s 改为 60s 减少开销

## 影响范围
- `docker/Dockerfile`
- `docker/Dockerfile.dev`
- `docker/docker-compose.yml`
