# fq-compressor

**C++23 FASTQ 压缩工具，提供 compress、decompress、verify 三个命令。**

[![CI 状态](https://github.com/LessUp/fq-compressor/actions/workflows/ci.yml/badge.svg)](https://github.com/LessUp/fq-compressor/actions/workflows/ci.yml)
[![GitHub Release](https://img.shields.io/github/v/release/LessUp/fq-compressor)](https://github.com/LessUp/fq-compressor/releases)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)
![CMake 3.28+](https://img.shields.io/badge/CMake-3.28%2B-blue.svg)
![Conan 2.x](https://img.shields.io/badge/Conan-2.x-blue.svg)

[中文](README.md) · [English](README.en.md) · [快速开始](#快速开始) · [架构](ARCHITECTURE.md) · [发布版本](https://github.com/LessUp/fq-compressor/releases)

## 功能

* `compress`：把 FASTQ 单端或双端数据压缩成 FQC 归档。
* `decompress`：把 FQC 归档还原为标准交错 FASTQ 流。
* `verify`：不解压输出，只做完整一致性校验。

## 快速开始

```bash
git clone https://github.com/LessUp/fq-compressor.git
cd fq-compressor
./scripts/build.sh clang-release
./build/clang-release/src/fqc --help
```

压缩：

```bash
./build/clang-release/src/fqc compress -i reads.fastq.gz -o reads.fqc
```

解压：

```bash
./build/clang-release/src/fqc decompress -i reads.fqc -o restored.fastq
```

校验：

```bash
./build/clang-release/src/fqc verify reads.fqc
```

双端：

```bash
./build/clang-release/src/fqc compress \
  -i reads_R1.fastq.gz \
  -2 reads_R2.fastq.gz \
  -o paired.fqc
```

更多参数见 `./build/clang-release/src/fqc --help`，格式与字节布局见 [ARCHITECTURE.md](ARCHITECTURE.md)。

## 技术栈

| 层级 | 技术 |
|---|---|
| 语言 | C++23（GCC 14+ / Clang 18+） |
| 构建 | CMake 3.28+ + Ninja |
| 包管理 | Conan 2.x |
| 压缩 | Zstd |
| 校验 | xxHash |
| 测试 | GoogleTest |

## 设计要点

* 顺序 frame：独立自校验 frame，一个引擎完成压缩、解压、校验。
* 紧凑编码：大写 A/C/G/T 用 2-bit 打包，其它 IUPAC 字符和小写碱基作为精确位置异常保留。
* 内存有界：默认 16 GiB 预算，最小 64 MiB；逐 frame 做保守峰值估算。
* 管道友好：支持 stdin/stdout；常规文件输出先写临时文件，成功后原子替换。
* 双端保持相邻：R1/R2 成对存储，frame 不会拆开 pair。
* 多层校验：全局头、每个逻辑 frame、结尾 footer 都带 XXH64。

完整说明见 [ARCHITECTURE.md](ARCHITECTURE.md)。

## 性能参考

环境：AMD Ryzen 7 5800H（WSL2，8 核 16 线程），Clang 21 Release，64 MiB 内存预算，随机合成数据，3 次重复 median。

| 场景 | 压缩 | 解压 | 峰值 RSS |
|---|---|---:|---:|
| 随机 Illumina-like 150 bp | 53.15 MiB/s | 182.40 MiB/s | 31.4 MiB |
| 随机 ONT-like 20 kbp | 55.66 MiB/s | 215.22 MiB/s | 25.5 MiB |

说明：WSL2 的 wall-clock 绝对值受噪声影响，以上数字更适合同一机器横向比较。完整数据见 [performance/INDEX.md](performance/INDEX.md)。

## 使用场景

适合：

* 需要一个顺序 FASTQ 归档/还原工具，且对内存敏感。
* 希望压缩、解压、校验走同一套格式和同一批边界。
* 需要接入管道或批处理流程。

不适合：

* 随机访问或按区间提取 reads。
* 有损压缩、原始顺序重排。
* 非 FASTQ 数据格式。

## 构建要求

* C++23 编译器：**GCC 14+** 或 **Clang 18+**
* **CMake 3.28+**
* **Conan 2.x**
* Linux / macOS；Windows 建议 WSL 或 Docker

## 质量

CI 包含：

* 格式：clang-format
* 静态分析：clang-tidy
* 编译：GCC Release、Clang Release
* 动态检查：ASan、TSan、UBSan
* 测试：单元、集成、端到端

## 文档

| 想做的事 | 看这里 |
|---|---|
| 构建与首次运行 | 本文件 |
| 命令参数 | `./build/clang-release/src/fqc --help` |
| 架构与格式 | [ARCHITECTURE.md](ARCHITECTURE.md) |
| 性能数据 | [performance/INDEX.md](performance/INDEX.md) |
| 变更记录 | [CHANGELOG.md](CHANGELOG.md) |

## 许可证

[MIT License](LICENSE)。
