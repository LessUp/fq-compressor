# CLion DevContainer 兼容性修复

## 日期
2026-01-23

## 变更类型
- 新增功能

## 变更内容

### 新增文件
- `.devcontainer/devcontainer.clion.json` - JetBrains CLion 专用 devcontainer 配置

### 修改文件
- `.devcontainer/README.md` - 添加 CLion 使用说明

## 问题背景

CLion 打开 devcontainer 时出现两个错误：
1. `bash: /.devcontainer/scripts/host-prepare.sh: No such file or directory`
2. `failed to solve: failed to read dockerfile: open Dockerfile.dev: no such file or directory`

**根因**：CLion 对 `${localWorkspaceFolder}` 变量和 Docker 构建上下文的处理方式与 VS Code 不同。

## 解决方案

创建 CLion 专用配置 `devcontainer.clion.json`：
- 移除 `initializeCommand`（CLion 支持有限）
- 使用 `build.dockerfile` + `build.context` 替代 `dockerComposeFile`
- 简化 `mounts` 配置，避免依赖 `host-prepare.sh` 创建的文件

## 使用说明

CLion 用户：
1. 打开项目目录
2. 选择 `File` → `Remote Development` → `Dev Containers`
3. 选择配置文件：`.devcontainer/devcontainer.clion.json`

如需同步宿主机配置（Git、Claude、Codex），请手动运行：
```bash
bash .devcontainer/scripts/host-prepare.sh
```
