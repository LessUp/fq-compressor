# DevContainer 宿主机数据目录挂载（2026-01-23）

## 背景

- 需要在 WSL 与远程服务器场景下方便配置宿主机数据目录挂载路径。

## 变更

- `docker/docker-compose.yml`
  - 为 dev 服务新增可配置的宿主机数据目录挂载（默认 `/tmp/fqcompressor-data`，容器内映射到 `/data`）。
- `docker/.env.example`
  - 增加 `FQCOMPRESSOR_HOST_DATA_DIR` 配置与示例说明。
- `.devcontainer/README.md`
  - 补充数据目录挂载的使用说明与示例。

## 影响

- WSL 与远程服务器可通过 `.env` 统一配置宿主机数据目录，并在容器内使用 `/data` 访问。

## 回退方案

- 回退上述文件到修改前版本，并删除本 changelog 文件。
