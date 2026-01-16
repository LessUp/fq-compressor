# 1. 删除旧容器
docker rm fqcompressor-dev

# 2. 确保 gitconfig 临时文件存在且是文件（不是目录）
rm -rf /tmp/fqcompressor-host-gitconfig
touch /tmp/fqcompressor-host-gitconfig

# 3. 如果有 ~/.gitconfig，复制过去
[ -f ~/.gitconfig ] && cp ~/.gitconfig /tmp/fqcompressor-host-gitconfig

# 4. 重新在 VS Code 中 Reopen in Container

# 用 sudo 强制删除
sudo rm -rf /tmp/fqcompressor-host-gitconfig

# 重新创建为文件
touch /tmp/fqcompressor-host-gitconfig

# 复制 gitconfig
[ -f ~/.gitconfig ] && cp ~/.gitconfig /tmp/fqcompressor-host-gitconfig
