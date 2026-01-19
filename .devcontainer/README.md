# FQCompressor DevContainer

本目录包含 VS Code DevContainer 配置，提供一致的 C++ 开发环境。

## 快速开始

### VS Code 用户

1. 安装 [Dev Containers](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers) 扩展
2. 打开项目目录
3. 按 `F1` → `Dev Containers: Reopen in Container`

#### 使用简化配置（devcontainer.simple.json）

当你希望跳过 docker-compose、直接用 Dockerfile 构建时：

1. 按 `F1` → `Dev Containers: Open Folder in Container...`
2. 选择配置文件：`.devcontainer/devcontainer.simple.json`

也可以使用 devcontainer CLI：

```bash
devcontainer up --workspace-folder . --config .devcontainer/devcontainer.simple.json
```

### 远程服务器（Windsurf/Cursor/Remote-SSH）

```bash
# 在服务器上启动容器
docker/start_devcontainer.sh --bind 0.0.0.0

# 然后通过 SSH 连接
ssh -p 2222 developer@<服务器IP>
```

## 文件说明

```
.devcontainer/
├── devcontainer.json        # 主配置（使用 docker-compose）
├── devcontainer.simple.json # 简化配置（直接构建）
├── setup-sshd.sh            # SSHD 配置脚本
├── start-sshd.sh            # SSHD 启动脚本
├── scripts/
│   ├── host-prepare.sh      # 宿主机准备脚本（initializeCommand）
│   └── container-setup.sh   # 容器内设置脚本（postXxxCommand）
└── README.md                # 本文件

docker/
├── Dockerfile.dev           # 开发环境镜像
├── docker-compose.yml       # Compose 配置
├── start_devcontainer.sh    # 手动启动脚本
├── .env                     # 环境变量
└── .env.example             # 环境变量模板
```

## 配置选项

### 环境变量 (docker/.env)

| 变量 | 默认值 | 说明 |
|------|--------|------|
| `USE_CHINA_MIRROR` | `0` | 启用国内镜像源 |
| `FQCOMPRESSOR_SSH_PORT` | `2222` | SSH 端口 |
| `FQCOMPRESSOR_SSH_BIND` | `127.0.0.1` | SSH 绑定地址 |

### 宿主机文件

devcontainer 会自动同步以下宿主机配置：

- `~/.gitconfig` → 容器内 Git 配置
- `~/.ssh/` → SSH 密钥（只读）
- `~/.claude/` → Claude Code 配置
- `~/.codex/` → OpenAI Codex 配置

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

### 网络代理

如需使用代理，编辑 `docker/.env`：
```bash
HTTP_PROXY=http://host.docker.internal:10808
HTTPS_PROXY=http://host.docker.internal:10808
```

## 开发工具

容器内预装：

- **编译器**: GCC 15 + Clang 21
- **构建**: CMake 4.x + Ninja + Conan 2.x
- **调试**: GDB + LLDB + Valgrind
- **分析**: clang-tidy + cppcheck + lcov
- **AI**: Claude Code CLI + OpenAI Codex

## 切换配置

- `devcontainer.json` - 使用 docker-compose，支持持久化缓存
- `devcontainer.simple.json` - 直接构建，更简单但无缓存持久化

切换方法：重命名文件或在 VS Code 中选择配置。
