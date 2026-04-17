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
- 🧬 **3.97× 压缩比**（Illumina 数据）
- ⚡ **11.9 MB/s** 压缩，**62.3 MB/s** 解压（多线程）
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

**其他平台：** 参见 [安装指南](https://lessup.github.io/fq-compressor/zh/getting-started/installation)

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
fqc compress -i reads_1.fastq -i2 reads_2.fastq \
  -o paired.fqc --paired

# 查看归档信息
fqc info reads.fqc
```

---

## 📊 性能基准测试

**测试数据集：** 2.27M Illumina reads（511 MB 未压缩）  
**硬件：** Intel Core i7-9700 @ 3.00GHz（8 核）

| 编译器 | 压缩速度 | 解压速度 | 压缩比 |
|--------|----------|----------|--------|
| **GCC 15.2** | 11.30 MB/s | 60.10 MB/s | **3.97×** |
| **Clang 21** | 11.90 MB/s | 62.30 MB/s | **3.97×** |

### 与其他工具对比

| 工具 | 压缩比 | 压缩速度 | 解压速度 | 并行 | 随机访问 |
|------|--------|----------|----------|------|----------|
| **fq-compressor** | **3.97×** | 11.9 MB/s | 62 MB/s | ✓ | ✓ |
| Spring | 4.5× | 3 MB/s | 25 MB/s | ✓ | ✗ |
| fqzcomp5 | 3.5× | 8 MB/s | 35 MB/s | ✗ | ✗ |
| gzip | 2.1× | 45 MB/s | 180 MB/s | ✗ | ✗ |
| zstd | 2.5× | 220 MB/s | 500 MB/s | ✗ | ✗ |

*更多对比参见 [详细基准测试](https://lessup.github.io/fq-compressor/zh/performance/benchmark)*

---

## 🔬 核心算法

### 基于组装的压缩（ABC）

fq-compressor 将 reads 视为**基因组片段**而非独立字符串：

1. **Minimizer 分桶** — 按共享 k-mer 签名分组 reads
2. **TSP 重排序** — 通过旅行商优化最大化相邻相似性
3. **共识生成** — 为每个桶构建局部共识序列
4. **增量编码** — 仅存储相对共识序列的编辑内容

### 统计上下文混合（SCM）

针对质量分数，fq-compressor 使用：

- **上下文：** 前序 QVs + 当前碱基 + 位置
- **预测：** 高阶上下文建模
- **编码：** 自适应算术编码
- **结果：** 质量数据接近熵极限的压缩

---

## 📚 文档

| 资源 | 描述 |
|------|------|
| [📖 在线文档](https://lessup.github.io/fq-compressor/) | 完整文档（中英双语） |
| [🚀 快速入门](https://lessup.github.io/fq-compressor/zh/getting-started/quickstart) | 5 分钟内开始 |
| [⌨️ CLI 参考](https://lessup.github.io/fq-compressor/zh/getting-started/cli-usage) | 完整命令参考 |
| [🏗️ 架构](https://lessup.github.io/fq-compressor/zh/core-concepts/architecture) | 高层设计 |
| [🧬 算法](https://lessup.github.io/fq-compressor/zh/core-concepts/algorithms) | 压缩算法深入解析 |
| [📋 FQC 格式](https://lessup.github.io/fq-compressor/zh/core-concepts/fqc-format) | 归档格式规范 |
| [🤝 贡献指南](https://lessup.github.io/fq-compressor/zh/development/contributing) | 开发者指南 |

---

## 🛠️ 开发

### 快速构建

```bash
# 调试构建
./scripts/build.sh gcc-debug

# 发布构建
./scripts/build.sh gcc-release

# 运行测试
./scripts/test.sh gcc-release
```

### 构建预设

| 预设 | 编译器 | 模式 | 消毒剂 |
|------|--------|------|--------|
| `gcc-debug` | GCC 15.2 | Debug | 无 |
| `gcc-release` | GCC 15.2 | Release | 无 |
| `clang-debug` | Clang 21 | Debug | 无 |
| `clang-asan` | Clang 21 | Debug | AddressSanitizer |
| `clang-tsan` | Clang 21 | Debug | ThreadSanitizer |
| `coverage` | GCC 15.2 | Debug | 覆盖率 |

### 代码质量

```bash
# 格式化代码
./scripts/lint.sh format

# 运行代码检查
./scripts/lint.sh lint clang-debug

# 全面检查
./scripts/lint.sh all clang-debug
```

---

## 🐳 Docker 使用

```bash
# 构建镜像
docker build -f docker/Dockerfile -t fq-compressor .

# 压缩数据
docker run --rm -v $(pwd):/data \
  fq-compressor compress -i /data/reads.fastq -o /data/reads.fqc

# 解压数据
docker run --rm -v $(pwd):/data \
  fq-compressor decompress -i /data/reads.fqc -o /data/restored.fastq
```

---

## 🗺️ 路线图

- [x] v0.1.0 - 初始发布，ABC + SCM 算法
- [x] v0.2.0 - 双端优化，长读支持
- [ ] 流式压缩模式
- [ ] 额外的有损质量模式
- [ ] AWS S3/GCS 原生集成
- [ ] Python 绑定

---

## 🤝 贡献

欢迎贡献！详情请参见我们的 [贡献指南](https://lessup.github.io/fq-compressor/zh/development/contributing)。

**贡献方式：**
- 🐛 报告错误和问题
- 💡 建议新功能
- 📝 改进文档
- 🔧 提交拉取请求
- 🧪 在不同数据集上测试

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
  <a href="CHANGELOG.zh-CN.md">变更日志</a> •
  <a href="https://github.com/LessUp/fq-compressor/discussions">讨论</a>
</p>
