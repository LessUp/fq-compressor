# 1. 删除旧容器
docker rm fqcompressor-dev

# 2. 清理宿主机缓存目录/文件
rm -rf ~/.fqcompressor-host-gitconfig \
       ~/.fqcompressor-host-claude \
       ~/.fqcompressor-host-codex

# 3. 重新运行宿主机准备脚本
bash .devcontainer/scripts/host-prepare.sh

# 4. 重新在 VS Code 中 Reopen in Container
