# DevContainer 环境优化

## 概述

根据最佳实践优化了 fq-compressor 项目的 DevContainer 开发环境配置。

## 变更文件

### 新增
- `.dockerignore` - 优化 Docker 构建上下文

### 修改
- `.devcontainer/devcontainer.json` - 添加 Features、增强扩展列表
- `.devcontainer/devcontainer.simple.json` - 同步配置改进
- `.devcontainer/README.md` - 更新文档结构和内容
- `docker/docker-compose.yml` - 添加 healthcheck、VS Code 扩展缓存卷
- `docker/Dockerfile.dev` - 添加标签、locale、便捷别名
- `docker/.env.example` - 添加更详细的配置说明

## 主要改进

### 1. DevContainer Features
- 添加 `common-utils` feature (Zsh + Oh My Zsh)
- 添加 `portsAttributes` 端口标签配置

### 2. VS Code 扩展增强
- GitLens, Git Graph - Git 可视化
- Docker - 容器管理
- Better C++ Syntax - 语法高亮
- Spell Checker - 拼写检查
- YAML, Markdown 支持

### 3. Clangd 优化
- `--suggest-missing-includes` - 建议缺失头文件
- `--all-scopes-completion` - 全作用域补全
- `--pch-storage=memory` - PCH 内存存储

### 4. Docker 配置增强
- `healthcheck` - 容器健康检查
- `init: true` - 正确的进程信号处理
- `security_opt` - 调试支持 (ptrace)
- VS Code 扩展持久化存储

### 5. 便捷命令
```bash
build-gcc      # GCC Debug 构建
build-clang    # Clang Debug 构建
build-release  # GCC Release 构建
```

## 验证

- [x] 所有脚本语法检查通过
