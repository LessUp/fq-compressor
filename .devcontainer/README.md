# FQCompressor DevContainer

本目录包含 DevContainer 配置，提供一致的 C++ 开发环境。

## 支持的开发环境

| 环境 | 推荐度 | 说明 |
|------|--------|------|
| **WSL2** | ⭐⭐⭐ 推荐 | Windows 上的最佳路径，原生文件系统性能 |
| **远程 Linux** | ⭐⭐⭐ 推荐 | 通过 `start_devcontainer.sh` 或 VS Code Remote-SSH |
| **Windows 原生** | ⚠️ 不推荐 | volume 性能极差，部分功能受限 |

## Windows 前置条件

> 远程 Linux 服务器用户可跳过此节，仅需 VS Code + Remote-SSH 扩展。

### 自动检查（推荐）

在 **PowerShell（管理员）** 中运行：

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
.\.devcontainer\scripts\host-prepare-windows.ps1
```

脚本会自动检查以下组件并提示安装缺失项。

### 手动安装清单

| 序号 | 组件 | 用途 | 安装方式 |
|------|------|------|----------|
| 1 | **WSL2 + Ubuntu** | Linux 文件系统，运行构建脚本 | `wsl --install` |
| 2 | **Docker Desktop** | 容器引擎（需启用 WSL2 后端） | [下载](https://www.docker.com/products/docker-desktop/) |
| 3 | **VS Code** | IDE | [下载](https://code.visualstudio.com/) |
| 4 | **VS Code 扩展** | 远程开发 | 见下方 |
| 5 | **Git** | 版本控制（含 Git Bash） | `winget install Git.Git` |

**必装 VS Code 扩展：**

```powershell
code --install-extension ms-vscode-remote.remote-wsl
code --install-extension ms-vscode-remote.remote-containers
code --install-extension ms-vscode-remote.remote-ssh
```

### Docker Desktop 配置

安装后需在 **Settings** 中确认：

- **General** → `Use the WSL 2 based engine` ✓
- **Resources → WSL Integration** → `Enable integration with my default WSL distro` ✓
- **Resources → Advanced**（建议）→ Memory ≥ 8 GB, CPUs ≥ 4

### WSL 内部准备

```bash
# 安装基础工具
sudo apt update && sudo apt install -y git curl

# 生成 SSH 密钥（如果没有）
ssh-keygen -t ed25519 -C "your_email@example.com"

# 克隆项目到 WSL 原生文件系统（重要！勿用 /mnt/c/）
mkdir -p ~/projects
git clone <repo-url> ~/projects/fq-compressor
cd ~/projects/fq-compressor

# 准备环境配置
cp docker/.env.example docker/.env
# 按需编辑 docker/.env
```

## 快速开始

### 方式一：WSL2 (推荐 Windows 路径)

```bash
# 1. 在 WSL 终端中克隆或打开项目（确保在 WSL 文件系统内，非 /mnt/c/）
cd ~/projects/fq-compressor

# 2. 准备环境变量
cp docker/.env.example docker/.env
# 编辑 docker/.env 按需修改

# 3. VS Code 打开
code .
# 按 F1 → "Dev Containers: Reopen in Container"
```

### 方式二：远程 Linux 服务器

```bash
# 在服务器上:
# 1. 准备环境变量
cp docker/.env.example docker/.env
vim docker/.env  # 设置 SSH_BIND=0.0.0.0 等

# 2. 启动容器
./docker/start_devcontainer.sh --bind 0.0.0.0

# 在本地:
# 3. SSH 连接
ssh -p 2222 developer@<服务器IP>

# 或使用 VS Code Remote-SSH → 连接后 Reopen in Container
```

### 方式三：JetBrains CLion

CLion 对 devcontainer 的支持与 VS Code 有所不同，请使用专用配置：

1. 打开项目目录
2. 选择 `File` → `Remote Development` → `Dev Containers`
3. 选择配置文件：`.devcontainer/devcontainer.clion.json`

> **注意**：首次使用前，如需同步宿主机配置（Git、Claude、Codex），请手动运行：
> ```bash
> bash .devcontainer/scripts/host-prepare.sh
> ```

### 方式四：Windows 原生 (不推荐)

```powershell
# 需要 Docker Desktop + Git Bash
# 性能差，强烈建议改用 WSL2

# 在 Git Bash 中:
cp docker/.env.example docker/.env
code .
# F1 → "Dev Containers: Reopen in Container"
```

> ⚠️ **Windows 原生注意事项**：
> - volume 挂载性能极差（比 WSL2 慢 5-10 倍）
> - `initializeCommand` 需要 Git Bash 中的 bash
> - 建议改用 WSL2：`code --remote wsl+Ubuntu /path/to/fq-compressor`

---

## 文件说明

```
.devcontainer/
├── devcontainer.json              # 主配置（使用 docker-compose，VS Code）
├── devcontainer.clion.json        # CLion 专用配置
├── scripts/
│   ├── host-prepare.sh            # 宿主机准备脚本（initializeCommand，含平台检测）
│   ├── host-prepare-windows.ps1   # Windows 环境检查/安装脚本（PowerShell）
│   ├── container-setup.sh         # 容器内设置脚本（postXxxCommand，平台感知）
│   ├── setup-sshd.sh              # SSHD 配置脚本
│   └── start-sshd.sh              # SSHD 启动脚本
└── README.md                      # 本文件

docker/
├── Dockerfile.dev           # 开发环境镜像（GCC 15 + Clang 22）
├── Dockerfile               # 生产构建镜像（多阶段：build + production）
├── Dockerfile.clang-release # CI 发布构建（Clang + libc++）
├── Dockerfile.glibc-release # CI 发布构建（glibc 动态链接）
├── Dockerfile.musl-release  # CI 发布构建（musl 完全静态链接）
├── docker-compose.yml       # Compose 配置（dev/prod/test/build 服务）
├── start_devcontainer.sh    # 手动启动脚本（远程服务器用）
└── .env.example             # 环境变量模板（复制为 .env 使用）
```

---

## 配置选项

### 环境变量 (docker/.env)

| 变量 | 默认值 | 说明 |
|------|--------|------|
| `USE_CHINA_MIRROR` | `0` | 启用国内镜像源 |
| `FQCOMPRESSOR_SSH_PORT` | `2222` | SSH 端口 |
| `FQCOMPRESSOR_SSH_BIND` | `127.0.0.1` | SSH 绑定地址（远程服务器设为 `0.0.0.0`） |
| `FQCOMPRESSOR_HOST_DATA_DIR` | `/tmp/fqcompressor-data` | dev 容器挂载到 `/data` 的宿主机路径 |
| `DEVCONTAINER_HTTP_PROXY` | (空) | HTTP 代理 |
| `DEVCONTAINER_HTTPS_PROXY` | (空) | HTTPS 代理 |

### 宿主机文件同步

devcontainer 会自动同步以下宿主机配置到容器内：

- **`~/.ssh/`** → SSH 密钥（只读挂载）
- **`~/.gitconfig`** → Git 配置（通过暂存文件同步）
- **`~/.claude/`** → Claude Code 配置
- **`~/.codex/`** → OpenAI Codex 配置

这些文件由 `host-prepare.sh`（initializeCommand）自动准备。

### 宿主机数据目录挂载

dev 容器会把宿主机的数据目录挂载到容器内 `/data`。

```bash
# docker/.env 中设置绝对路径（~ 不会展开）

# WSL2
FQCOMPRESSOR_HOST_DATA_DIR=/home/<user>/data

# 远程服务器
FQCOMPRESSOR_HOST_DATA_DIR=/data
```

`start_devcontainer.sh` 可用 `--data-path` 临时覆盖。

### 网络代理配置

代理配置因平台而异，编辑 `docker/.env`：

```bash
# WSL2 / Windows Docker Desktop — 使用 host.docker.internal 访问宿主机代理
DEVCONTAINER_HTTP_PROXY=http://host.docker.internal:10808
DEVCONTAINER_HTTPS_PROXY=http://host.docker.internal:10808

# 远程 Linux 服务器 — host.docker.internal 默认不可用，直接用本机地址
DEVCONTAINER_HTTP_PROXY=http://127.0.0.1:10808
DEVCONTAINER_HTTPS_PROXY=http://127.0.0.1:10808
```

---

## 开发工具

容器内预装：

| 类别 | 工具 |
|------|------|
| **编译器** | GCC 15 + Clang 22 |
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

## 故障排除

### 构建失败：bind mount 文件/目录类型错误

```bash
# 删除错误的占位文件/目录并重新准备
rm -rf ~/.fqcompressor-host-gitconfig ~/.fqcompressor-host-claude ~/.fqcompressor-host-codex
bash .devcontainer/scripts/host-prepare.sh
```

### SSH 连接失败（远程服务器）

```bash
# 1. 检查 SSHD 是否运行
docker exec -it <container> pgrep -x sshd

# 2. 检查 authorized_keys
docker exec -it <container> cat /home/developer/.ssh_authorized_keys

# 3. 查看 SSHD 日志
docker exec -it <container> sudo cat /var/log/auth.log
```

### WSL2 volume 性能差

确保项目在 **WSL 原生文件系统** 内（`/home/...`），**而非** Windows 挂载路径（`/mnt/c/...`）：

```bash
# ✅ 正确: WSL 原生路径
cd ~/projects/fq-compressor

# ❌ 错误: Windows 挂载路径（性能差 5-10 倍）
cd /mnt/c/Users/.../fq-compressor
```

### 扩展安装缓慢

VS Code 扩展已配置持久化存储，重建容器后无需重新下载。如需清理：

```bash
docker volume rm fqcompressor_vscode_extensions
```

### Docker 未运行

```bash
# WSL2: Docker Desktop 需要先启动
# 远程 Linux: 检查 dockerd 状态
sudo systemctl status docker
```

---

## 切换 IDE 配置

- **`devcontainer.json`** - 使用 docker-compose，支持持久化缓存（VS Code / Windsurf / Cursor）
- **`devcontainer.clion.json`** - CLion 专用配置，含调试安全选项和缓存卷

切换方法：在 IDE 中选择对应的配置文件。
