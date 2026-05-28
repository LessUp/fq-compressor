Devcontainer 的主说明、故障排查和平台差异现在统一维护在
`.devcontainer/README.md`。

如果你只是需要重置宿主机同步缓存，可直接执行：

```bash
docker rm fqcompressor-dev
rm -rf ~/.fqcompressor-host-gitconfig \
       ~/.fqcompressor-host-claude \
       ~/.fqcompressor-host-codex
bash .devcontainer/scripts/host-prepare.sh
```

之后按 `.devcontainer/README.md` 中的对应 IDE 流程重新进入容器。
