# fq-compressor

用 C++23 写的 FASTQ 命令行压缩工具。

fq-compressor 把 FASTQ 测序数据无损压缩成紧凑的 FQC 归档，再完整还原。它保留 read ID、注释、序列和质量值；支持单端和双端数据；直接接受 `.gz` 输入；也能通过 stdin/stdout 接入管道。

## 它是做什么的

这个工具只聚焦一件事：把 FASTQ 文件顺序归档，并在需要时原样恢复。不追求随机访问、全局重排或额外的有损模式，换来的是稳定的吞吐、可控的内存占用和清晰的错误边界。

编码方式很直接：

- ID/注释、序列、质量值分三路独立处理；
- 大写 A/C/G/T 用 2-bit 打包，其它 IUPAC 字符和小写碱基作为精确位置异常保留；
- 三路流都用 varint 分帧 + Zstd level 1 压缩；
- 全局头、每个 frame、结尾 footer 都带 XXH64 校验。

支持 `illumina`、`ont`、`pacbio-hifi`、`pacbio-clr` 四类测序类型，可自动检测，也可以显式指定。目前统一使用一套已经验证的编码；只有当某个专用 codec 在真实基准数据上稳定减小体积且不退化时，才会被纳入。

## 用法示例

```bash
# 单端数据，测序类型自动检测
fqc compress -i reads.fastq -o reads.fqc

# 双端数据：R1/R2 成对保存，frame 不会拆开 pair
fqc compress -i reads_R1.fastq -2 reads_R2.fastq -o paired.fqc

# 显式指定长读类型
fqc compress -i ont.fastq.gz -o ont.fqc --profile ont

# 校验与解压
fqc verify reads.fqc
fqc decompress -i reads.fqc -o restored.fastq

# 接入管道
producer | fqc compress -i - -o - | consumer
fqc decompress -i reads.fqc -o - | downstream-tool
```

全局选项可放在子命令前后：

```text
--memory-limit MiB   操作内存预算，默认 16384，最小 64
-q, --quiet          只输出错误
```

压缩额外支持 `--profile`、`--frame-mib`、`--force`；解压支持 `--force`。双端归档解压后是一份标准交错 FASTQ 流。

## 构建

要求：CMake 3.28+、Conan 2.x、GCC 14+ 或 Clang 18+。

```bash
conan profile detect --force
./scripts/build.sh clang-release
```

二进制在 `build/clang-release/src/fqc`。

## 格式说明

FQC 归档由带校验的全局头、若干独立 frame 和结尾 footer 组成。读取时会拒绝版本或 codec 不符、frame 超限、pair 数量不一致、截断、footer 后残留数据以及 checksum 错误。字节级布局见 [`ARCHITECTURE.md`](ARCHITECTURE.md)。

## 实测性能

在 8 核 AMD Ryzen 7 5800H x86_64 上，release CLI 端到端测得：

| 合成数据 | 压缩 | 解压 | 64 MiB 预算下峰值 RSS |
|---|---:|---:|---:|
| 随机 Illumina-like 150 bp | 53.15 MiB/s | 182.40 MiB/s | 31.4 MiB |
| 随机 ONT-like 20 kbp | 55.66 MiB/s | 215.22 MiB/s | 25.5 MiB |

这些数字来自 WSL2 环境的三次中位数，计入了解析、校验、frame 构建、I/O 和逐字节比对。它们不是真实生物数据的压缩率承诺，也不能当作所有平台的稳定保证。当前版本已在 x86_64（Linux glibc/musl、macOS Intel）上完成测试和发布。

本地复现：

```bash
FQC_BIN=build/clang-release/src/fqc \
FQC_PERF_SIZES="64 256" \
FQC_PERF_DATA=random \
FQC_PERF_ENFORCE_SLA=1 \
bash tests/e2e/test_performance.sh
```

更完整的性能记录见 [`performance/INDEX.md`](performance/INDEX.md)。

## 适用范围

正式发布的产品只有 `fqc` 命令行可执行文件。`include/fqc` 下的头文件以及 `fqc_core`/`fqc_cli` CMake target 是源码构建和测试用的内部模块边界，**不构成受支持的 C++ API、ABI 或 CMake 包**，也不保证兼容性。

当前发布包覆盖 x86_64：Linux glibc、静态 Linux musl、macOS Intel。

## 开发与验证

```bash
./scripts/test.sh clang-debug
./scripts/lint.sh format-check
```

测试覆盖 FQC 字节格式、损坏/截断、内存准入、parser/I/O、单端/双端往返、stdin/stdout 和真实 CLI 进程边界。

## 许可证

MIT，见 [`LICENSE`](LICENSE)。
