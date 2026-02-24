# FQCompressor DevContainer

本目录包含 VS Code DevContainer 配置，提供一致的 C++ 开发环境。

## 快速开始

### VS Code 用户

1. 安装 [Dev Containers](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers) 扩展
2. 打开项目目录
3. 按 `F1` → `Dev Containers: Reopen in Container`

### JetBrains CLion 用户

CLion 对 devcontainer 的支持与 VS Code 有所不同，请使用专用配置：

1. 打开项目目录
2. 选择 `File` → `Remote Development` → `Dev Containers`
3. 选择配置文件：`.devcontainer/devcontainer.clion.json`

> **注意**：首次使用前，如需同步宿主机配置（Git、Claude、Codex），请手动运行：
> ```bash
> bash .devcontainer/scripts/host-prepare.sh
> ```

### 远程服务器（Windsurf/Cursor/Remote-SSH）

```bash
# 在服务器上启动容器
docker/start_devcontainer.sh --bind 0.0.0.0

# 然后通过 SSH 连接
ssh -p 2222 developer@<服务器IP>
```

---

## 文件说明

```
.devcontainer/
├── devcontainer.json        # 主配置（使用 docker-compose）
├── devcontainer.clion.json  # CLion 专用配置
├── scripts/
│   ├── host-prepare.sh      # 宿主机准备脚本（initializeCommand）
│   ├── container-setup.sh   # 容器内设置脚本（postXxxCommand）
│   ├── setup-sshd.sh        # SSHD 配置脚本
│   └── start-sshd.sh        # SSHD 启动脚本
└── README.md                # 本文件

docker/
├── Dockerfile.dev           # 开发环境镜像
├── Dockerfile               # 生产构建镜像
├── docker-compose.yml       # Compose 配置
├── start_devcontainer.sh    # 手动启动脚本
├── .env                     # 环境变量
└── .env.example             # 环境变量模板
```

---

## 配置选项

### 环境变量 (docker/.env)

| 变量 | 默认值 | 说明 |
|------|--------|------|
| `USE_CHINA_MIRROR` | `0` | 启用国内镜像源 |
| `FQCOMPRESSOR_SSH_PORT` | `2222` | SSH 端口 |
| `FQCOMPRESSOR_SSH_BIND` | `127.0.0.1` | SSH 绑定地址 |
| `FQCOMPRESSOR_HOST_DATA_DIR` | `/tmp/fqcompressor-data` | 宿主机数据目录（映射到 `/data`） |

### 宿主机文件

devcontainer 会自动同步以下宿主机配置：

- `~/.gitconfig` → 容器内 Git 配置
- `~/.ssh/` → SSH 密钥（只读）
- `~/.claude/` → Claude Code 配置
- `~/.codex/` → OpenAI Codex 配置

---

## 开发工具

容器内预装：

| 类别 | 工具 |
|------|------|
| **编译器** | GCC 15 + Clang 21 |
| **构建** | CMake 4.x + Ninja + Conan 2.x |
| **调试** | GDB + LLDB + Valgrind |
| **分析** | clang-tidy + cppcheck + lcov |
| **AI** | Claude Code CLI + OpenAI Codex |

### 便捷命令

容器内提供以下快捷命令：

```bash
# 构建命令
build-gcc      # cmake --preset gcc-debug && cmake --build --preset gcc-debug
build-clang    # cmake --preset clang-debug && cmake --build --preset clang-debug
build-release  # cmake --preset gcc-release && cmake --build --preset gcc-release

# Claude Code 快捷方式
claude-duck    # 使用 Duck 配置
claude-glm     # 使用 GLM 配置
claude-kimi    # 使用 Kimi 配置
```

---

## 最佳实践

### 1. 使用 WSL2（Windows 用户）

推荐在 WSL2 中打开项目，再使用 "Reopen in Container"：

```bash
# 在 WSL2 中
cd /path/to/fq-compressor
code .
```

### 2. SSH Agent 转发

确保 SSH agent 在宿主机运行：

```bash
# WSL2/Linux
eval "$(ssh-agent -s)"
ssh-add ~/.ssh/id_ed25519
```

### 3. 数据目录

配置 `FQCOMPRESSOR_HOST_DATA_DIR` 挂载大数据集：

```bash
# 编辑 docker/.env
FQCOMPRESSOR_HOST_DATA_DIR=/path/to/your/data
```

容器内使用 `/data` 访问：

```bash
fqc compress /data/sample.fastq -o /data/output.fqc
```

---

## 故障排除

### 构建失败：文件/目录类型错误

如果看到类似错误：
```
ERROR: ~/.fqcompressor-host-gitconfig 是目录，会导致 devcontainer bind mount 失败
```

运行以下命令修复：
```bash
# 删除错误的目录
rm -rf ~/.fqcompressor-host-gitconfig ~/.fqcompressor-host-claude ~/.fqcompressor-host-codex

# 重新运行准备脚本
bash .devcontainer/scripts/host-prepare.sh
```

### SSH 连接失败

1. 检查 SSHD 是否运行：
   ```bash
   docker exec -it <container> pgrep -x sshd
   ```

2. 检查 authorized_keys：
   ```bash
   docker exec -it <container> cat /home/developer/.ssh_authorized_keys
   ```

3. 查看 SSHD 日志：
   ```bash
   docker exec -it <container> sudo cat /var/log/auth.log
   ```

### 扩展安装缓慢

VS Code 扩展已配置持久化存储，重建容器后无需重新下载。如需清理：

```bash
docker volume rm fqcompressor_vscode_extensions
```

### 网络代理

如需使用代理，编辑 `docker/.env`：
```bash
DEVCONTAINER_HTTP_PROXY=http://host.docker.internal:10808
DEVCONTAINER_HTTPS_PROXY=http://host.docker.internal:10808
```

---

## 切换配置

- `devcontainer.json` - 使用 docker-compose，支持持久化缓存（VS Code）
- `devcontainer.clion.json` - CLion 专用配置，解决路径解析兼容性问题

切换方法：在 IDE 中选择对应的配置文件。
