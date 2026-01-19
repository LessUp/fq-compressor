# DevContainer/Docker 同步更新（2026-01-18）

## 背景

为确保 fq-compressor 与参考项目的开发容器/镜像配置保持一致，需要同步 DevContainer 脚本、镜像构建与 SSH 访问策略。

## 变更

- `.devcontainer/`
  - 新增主机/容器脚本与简化配置，统一生命周期钩子与配置同步策略。
  - 同步 SSHD 配置与启动脚本，支持环境变量控制端口与公钥刷新。
  - 补充 DevContainer 使用说明文档。
- `docker/`
  - 更新 `Dockerfile.dev` 工具链与开发工具，补齐 Claude/Codex CLI 支持。
  - 新增生产镜像 `Dockerfile`，对齐构建流程与运行时依赖。
  - 更新 `docker-compose.yml`、`.env`、`.env.example` 与 `start_devcontainer.sh`，统一 SSH 端口与代理/镜像源配置。
- `help.md`
  - 替换旧的 /tmp 临时文件流程，改用新的宿主机准备脚本。

## 影响

- DevContainer 初始化与 SSH 登录流程更稳定，支持 Claude/Codex 配置同步。
- Docker 镜像与 compose 入口统一，便于本地/远程一致启动。

## 回退方案

- 回退本次修改的 DevContainer/Docker 文件与脚本版本。
