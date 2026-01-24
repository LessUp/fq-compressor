# DevContainer 代理变量修复（2026-01-22）

## 背景

DevContainer 构建阶段意外继承宿主机的 HTTP_PROXY，导致 apt 访问 127.0.0.1:10808 失败。

## 变更

- `docker/docker-compose.yml`
  - 将构建代理变量切换为 `DEVCONTAINER_*`，避免自动继承宿主机代理。
- `docker/.env.example`
  - 更新代理示例为 `DEVCONTAINER_*` 变量。
- `.devcontainer/devcontainer.simple.json`
  - 显式传递 `DEVCONTAINER_*` 构建参数。
- `.devcontainer/README.md`
  - 更新代理配置说明。

## 影响

- 未配置 `DEVCONTAINER_*` 时，构建不再强制走宿主机代理。

## 回退方案

- 回退以上文件到修改前版本。
