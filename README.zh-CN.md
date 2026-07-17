# fq-compressor

`fq-compressor` 是一个使用 C++23 实现的 FASTQ 压缩工具，采用顺序、可流式处理、受内存
上限约束的 FQC v2 格式。它无损保存 read ID、注释、序列、质量值以及双端 reads 的相邻
关系，并可直接读取普通 FASTQ 和 `.gz` FASTQ。

> FQC v2 有意不兼容旧 v1 格式和旧 CLI。不提供 v1 reader、迁移命令、随机访问、有损模式
> 或原始顺序映射。

## 为什么重构为 v2

旧 ABC/SCM 实现同时存在昂贵的全局分析、每线程稠密质量模型，以及两套相互分叉的串行/并行
引擎；已追踪吞吐约为 0.1 MiB/s。v2 改为彼此独立的顺序 frame，并由同一个引擎完成压缩、
解压和完整校验。

当前生产 fallback：

- ID/注释以 varint 分帧，再用 Zstd level 1 压缩；
- 大写 A/C/G/T 使用 2-bit 编码，其他 IUPAC 字符和小写字符以精确位置异常表保存，再用
  Zstd level 1 压缩；
- 质量值以 varint 分帧，再用 Zstd level 1 压缩；
- 全局头、每个逻辑 frame 和 footer 汇总均使用 XXH64 校验。

支持 `illumina`、`ont`、`pacbio-hifi`、`pacbio-clr` 四类 profile，可自动检测或显式指定。
目前四类 profile 共用已经验证的 fallback codec。专用 codec 只有在保持吞吐门槛、基准语料库
中位体积至少改善 5%，且任何单个数据集退化不超过 1% 时才允许进入生产路径。

## 构建

要求：CMake 3.28+、Conan 2.x、GCC 14+ 或 Clang 18+。

```bash
conan profile detect --force
./scripts/build.sh clang-release
```

二进制位于 `build/clang-release/src/fqc`。

## 支持边界

正式发布的产品仅为 `fqc` 命令行可执行文件，安装目录和 Conan 包只包含 `bin/fqc`。
`include/fqc` 下的头文件以及 `fqc_core`/`fqc_cli` CMake target 只是源码构建和测试使用的内部
模块边界；它们不构成受支持的 C++ API、ABI 或 CMake package，也不承诺兼容性。

## CLI

```bash
# 单端数据，自动检测 profile
fqc compress -i reads.fastq -o reads.fqc

# 双端文件；每个 R1/R2 pair 保持相邻，frame 不会拆开 pair
fqc compress -i reads_R1.fastq -2 reads_R2.fastq -o paired.fqc

# 显式指定长读 profile
fqc compress -i ont.fastq.gz -o ont.fqc --profile ont

# 完整校验和解压
fqc verify reads.fqc
fqc decompress -i reads.fqc -o restored.fastq

# stdin/stdout 管道
producer | fqc compress -i - -o - | consumer
fqc decompress -i reads.fqc -o - | downstream-tool
```

全局参数可放在子命令前或后：

```text
--memory-limit MiB   操作硬预算；默认 16384，最小 64
-q, --quiet          仅输出错误
```

压缩还支持 `--profile`、`--frame-mib`、`--force`；解压支持 `--force`。双端归档解压为一个
标准交错 FASTQ 流。

## 格式与兼容性

FQC v2 由带校验的全局头、零到多个独立 frame 和结束 footer 组成。reader 会拒绝未知版本或
codec、超限 frame、不完整 pair、截断、逻辑流或 footer 后的尾随数据和 checksum 不一致。它
不会探测或解码 v1。

字节布局与内存模型见 [ARCHITECTURE.md](ARCHITECTURE.md)。

## 已测性能

在 8 核 AMD Ryzen 7 5800H x86_64 上使用 release CLI 测量；数据包含 FASTQ 解析、checksum、
frame 构建、文件 I/O、解压和逐字节一致性检查。

| 合成数据 | 压缩 | 解压 | 64 MiB 预算下峰值 RSS |
|---|---:|---:|---:|
| 随机化 Illumina-like 150 bp | 53.15 MiB/s | 182.40 MiB/s | 31.4 MiB |
| 随机化 ONT-like 20 kbp | 55.66 MiB/s | 215.22 MiB/s | 25.5 MiB |

这些数字是 [performance/INDEX.md](performance/INDEX.md) 所记录 WSL2 环境的固定三次中位数，
不用于宣称真实生物数据压缩率，也不是稳定的发布机保证。WSL2 墙钟波动曾使重跑结果下降，
其中 Illumina 压缩低于 50 MiB/s。仓库目前没有足够的真实 ONT/HiFi/CLR 语料来准入专用长读
codec；ARM64 仍需在发布机上单独验证。表格包含完整逻辑记录校验，并记录各轮最大 RSS。

```bash
FQC_BIN=build/clang-release/src/fqc \
FQC_PERF_SIZES="64 256" \
FQC_PERF_DATA=random \
FQC_PERF_ENFORCE_SLA=1 \
bash tests/e2e/test_performance.sh
```

`tests/e2e/test_performance.sh` 用于吞吐、RSS 和 SLA 测量；用户可见的数据集同类工具对比（配置
后包括 Spring）统一使用 `./scripts/benchmark.sh`。
codec 的准入/拒绝结论记录在 [benchmark_v2/CODEC_GATES.md](benchmark_v2/CODEC_GATES.md)。

## 开发与验证

项目处于 closeout 模式：优先完成、简化和稳定，不保留重复路径。

```bash
./scripts/test.sh clang-debug
./scripts/lint.sh format-check
```

测试覆盖 v2 字节格式、损坏/截断、内存准入、parser/I/O、单端/双端往返、stdin/stdout 和真实
CLI 进程边界。

## 许可证

MIT，见 [LICENSE](LICENSE)。
