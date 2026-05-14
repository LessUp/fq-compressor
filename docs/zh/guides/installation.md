# 安装

当你需要用最短路径拿到可用的 `fqc` 二进制时，读这一页即可。
安装完成后，如需做第一次可验证往返，请继续看 [快速开始](./getting-started)。

## Linux 发布二进制

当前 GitHub Releases 提供 Linux tarball。

```bash
wget https://github.com/LessUp/fq-compressor/releases/download/v0.2.0/fq-compressor-v0.2.0-linux-x86_64-musl.tar.gz
tar -xzf fq-compressor-v0.2.0-linux-x86_64-musl.tar.gz
sudo mv fq-compressor-v0.2.0-linux-x86_64-musl/fqc /usr/local/bin/
fqc --version
```

## 从源码构建

当你需要当前仓库状态、调试构建，或本地修改版本时，走这条路径。

```bash
git clone https://github.com/LessUp/fq-compressor.git
cd fq-compressor
./scripts/build.sh clang-debug
build/clang-debug/src/fqc --version
```

## 工具链要求

- GCC 14+ 或 Clang 18+
- CMake 3.28+
- Conan 2.x

## 继续阅读

- [快速开始](./getting-started)：做一次烟雾级归档往返
- [CLI 使用](./cli-usage)：查看命令面
- [Releases](https://github.com/LessUp/fq-compressor/releases)：查看发布工件
