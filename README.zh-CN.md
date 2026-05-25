# fq-compressor

<p align="center">
  <b>面向测序时代的高性能 FASTQ 压缩工具</b>
</p>

<p align="center">
  <a href="https://github.com/LessUp/fq-compressor/actions/workflows/ci.yml">
    <img src="https://github.com/LessUp/fq-compressor/actions/workflows/ci.yml/badge.svg" alt="CI 状态">
  </a>
  <a href="https://github.com/LessUp/fq-compressor/actions/workflows/quality.yml">
    <img src="https://github.com/LessUp/fq-compressor/actions/workflows/quality.yml/badge.svg" alt="代码质量">
  </a>
  <a href="https://github.com/LessUp/fq-compressor/releases/latest">
    <img src="https://img.shields.io/github/v/release/LessUp/fq-compressor?include_prereleases&label=发布版本" alt="最新发布">
  </a>
  <a href="LICENSE">
    <img src="https://img.shields.io/badge/许可证-MIT-green.svg" alt="许可证">
  </a>
  <a href="https://en.cppreference.com/w/cpp/23">
    <img src="https://img.shields.io/badge/C%2B%2B-23-blue.svg" alt="C++23">
  </a>
  <a href="https://lessup.github.io/fq-compressor/">
    <img src="https://img.shields.io/badge/文档-在线-blue?logo=gitbook" alt="文档">
  </a>
</p>

<p align="center">
  <a href="README.md">English</a> •
  <a href="README.zh-CN.md">简体中文</a> •
  <a href="https://github.com/LessUp/fq-compressor-rust">Rust 实现</a>
</p>

---

## 🎯 什么是 fq-compressor？

**fq-compressor** 是一款高性能的 FASTQ 压缩工具，利用**基于组装的压缩（ABC）**和**统计上下文混合（SCM）**技术，在保持对压缩数据 **O(1) 随机访问** 的同时，实现接近熵极限的压缩比。

**核心亮点：**
- 🧪 通过 `./scripts/benchmark_v2.sh` 生成 **证据优先** 的 benchmark JSON 与 Markdown 结论
- 📊 自动产出压缩比、压缩速度、解压速度的 **性能站位**
- 🎯 **随机访问**，无需完整解压
- 🚀 **Intel oneTBB** 并行流水线
- 📦 **透明支持** .gz、.bz2、.xz 输入

---

## 📦 快速安装

### 预编译二进制（推荐）

**Linux（x86_64，静态二进制）：**
```bash
wget https://github.com/LessUp/fq-compressor/releases/download/v0.2.0/fq-compressor-v0.2.0-linux-x86_64-musl.tar.gz
tar -xzf fq-compressor-v0.2.0-linux-x86_64-musl.tar.gz
sudo mv fq-compressor-v0.2.0-linux-x86_64-musl/fqc /usr/local/bin/
```

**macOS（Homebrew）：**
```bash
# 即将推出
```

**其他平台：** 参见 [安装指南](https://lessup.github.io/fq-compressor/zh/academy/installation)

### 从源码构建

```bash
git clone https://github.com/LessUp/fq-compressor.git
cd fq-compressor

# 通过 Conan 安装依赖
conan install . --build=missing -of=build/gcc-release \
    -s build_type=Release -s compiler.cppstd=23

# 构建
cmake --preset gcc-release
cmake --build --preset gcc-release -j$(nproc)

# 二进制位置：build/gcc-release/src/fqc
```

**前置条件：** GCC 14+ 或 Clang 18+，CMake 3.28+，Conan 2.x

---

## 🚀 基本用法

### 压缩与解压

```bash
# 将 FASTQ 压缩为 FQC 格式
fqc compress -i reads.fastq -o reads.fqc

# 验证归档完整性
fqc verify reads.fqc

# 完整解压
fqc decompress -i reads.fqc -o restored.fastq
```

### 高级功能

```bash
# 随机访问 - 提取第 1000-2000 条 reads
fqc decompress -i reads.fqc --range 1000:2000 -o subset.fastq

# 多线程压缩（8 线程）
fqc compress -i reads.fastq -o reads.fqc -t 8 -v

# 双端数据
fqc compress -i reads_1.fastq -2 reads_2.fastq \
  -o paired.fqc --paired

# 查看归档信息
fqc info reads.fqc
```

---

## 📊 核心证明点

- **压缩密度应以生成的 benchmark 报告为准**，而 **O(1) 随机访问** 仍是系统契约的一部分
- 最新 benchmark 证据由 `./scripts/benchmark_v2.sh` 生成，产出 JSON 与 Markdown 报告
- 当前性能区间应以生成的报告为准，而不是继续依赖 README 中写死的常数
- 通过 `fqc info` 和 `fqc verify` 提供**归档检查与完整性验证**
- **透明支持** `.gz`、`.bz2`、`.xz` FASTQ 输入

更详细的性能数据、算法说明和格式设计请查看维护中的文档，而不是继续把 README 扩成第二个站点首页。

---

## 📚 文档与项目表面

| 表面 | 角色 |
|------|------|
| [📖 GitHub Pages](https://lessup.github.io/fq-compressor/) | 对外 landing page 与中英文入口 |
| [English docs](https://lessup.github.io/fq-compressor/en/) | 白皮书、学院、架构与证据 |
| [简体中文文档](https://lessup.github.io/fq-compressor/zh/) | 白皮书、学院、架构说明、证据链 |
| [📦 发布版本](https://github.com/LessUp/fq-compressor/releases) | 预编译二进制下载 |
| [🤝 贡献指南](https://lessup.github.io/fq-compressor/zh/academy/contributing) | 面向 closeout 阶段的开发流程 |

---

## 🛠️ 开发

`fq-compressor` 当前处于 **closeout mode**。简化的开发流程：

```bash
./scripts/build.sh clang-debug
./scripts/lint.sh format-check
./scripts/test.sh clang-debug
bash tests/e2e/benchmark_v2_smoke_test.sh
```

生成可复现 benchmark 证据：

```bash
./scripts/benchmark_v2.sh run \
  --workload small20k-single \
  --data-root /home/shane/data/test \
  --tools fqc,gzip,xz,bzip2 \
  --threads 1 \
  --runs 1 \
  --json build/benchmark_v2/small20k-single.json \
  --report build/benchmark_v2/small20k-single.md
```

详见 AGENTS.md 获取完整项目规则和架构说明。

---

## 🤝 贡献

欢迎面向收尾阶段的聚焦贡献，尤其包括：

- 文档清理与职责边界收紧
- 基于证据的缺陷修复与回归覆盖
- 工作流与工具链精简
- archive-readiness 打磨

具体流程请参阅 [贡献指南](https://lessup.github.io/fq-compressor/zh/academy/contributing)。

---

## 📄 许可证

- **项目代码：** MIT 许可证 — 参见 [LICENSE](LICENSE)
- **vendor/spring-core/：** Spring 原始研究许可证（非 MIT）

---

## 🙏 致谢

- **Spring** ([Chandak 等，2019](https://doi.org/10.1101/gr.234583.119)) — ABC 算法灵感
- **fqzcomp5** (Bonfield) — 质量压缩参考
- **Intel oneTBB** — 并行计算框架
- **贡献者** — 所有帮助改进本项目的人

---

<p align="center">
  <a href="https://github.com/LessUp/fq-compressor/releases">发布版本</a> •
  <a href="https://lessup.github.io/fq-compressor/">文档</a> •
  <a href="CHANGELOG.md">变更日志</a> •
  <a href="https://github.com/LessUp/fq-compressor/discussions">讨论</a>
</p>
