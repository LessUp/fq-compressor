# Phase 4: TBB 并行优化 - 使用指南

## 概述

Phase 4 实现了基于 Intel TBB `parallel_pipeline` 的多线程压缩/解压功能，可以充分利用多核 CPU 提升性能。

## 快速开始

### 1. 编译项目

```bash
# 使用 GCC 编译 Release 版本
./scripts/build.sh gcc-release

# 或使用 Clang
./scripts/build.sh clang-release
```

### 2. 基本用法

#### 自动线程检测（推荐）

```bash
# 压缩（自动检测 CPU 核心数）
./build/gcc-release/fq-compressor compress input.fastq -o output.fqc

# 解压（自动检测 CPU 核心数）
./build/gcc-release/fq-compressor decompress output.fqc -o output.fastq
```

#### 指定线程数

```bash
# 使用 4 线程压缩
./build/gcc-release/fq-compressor compress input.fastq -o output.fqc --threads 4

# 使用 8 线程解压
./build/gcc-release/fq-compressor decompress output.fqc -o output.fastq --threads 8
```

#### 强制单线程模式

```bash
# 单线程压缩（用于对比或调试）
./build/gcc-release/fq-compressor compress input.fastq -o output.fqc --threads 1
```

## 性能基准测试

### 运行基准测试

```bash
# 测试 1, 2, 4, 8 线程的性能
./benchmark/benchmark_parallel.sh input.fastq 1 2 4 8

# 测试更多线程数
./benchmark/benchmark_parallel.sh input.fastq 1 4 8 16 32
```

### 查看结果

测试完成后，结果保存在 `./benchmark_results/` 目录：

- `parallel_benchmark_YYYYMMDD_HHMMSS.csv` - CSV 格式的原始数据
- `parallel_benchmark_YYYYMMDD_HHMMSS.md` - Markdown 格式的报告

## 性能调优

### 1. 选择合适的线程数

**推荐配置**:

| CPU 核心数 | 建议线程数 | 说明 |
|------------|------------|------|
| 2-4 核 | 自动检测 | 小型系统，让程序自动选择 |
| 4-8 核 | 4-6 线程 | 中型系统，留一些核心给系统使用 |
| 8-16 核 | 8-12 线程 | 大型系统，平衡 CPU 和 I/O |
| 16+ 核 | 16 线程 | 高端系统，配合 NVMe SSD 使用 |

**注意**: 过多的线程数不一定能提升性能，可能会因为线程切换和锁竞争导致性能下降。

### 2. 调整块大小

块大小影响内存使用和并行效率：

```bash
# 小块（更高并行度，但开销更大）
./build/gcc-release/fq-compressor compress input.fastq -o output.fqc \
    --threads 8 --block-size 50000

# 大块（更低开销，但并行度受限）
./build/gcc-release/fq-compressor compress input.fastq -o output.fqc \
    --threads 8 --block-size 200000
```

**推荐**: 使用默认值（100K reads for short reads）即可，程序会根据 read length 自动调整。

### 3. 内存限制

如果系统内存有限，可以设置内存上限：

```bash
# 限制最多使用 4GB 内存
./build/gcc-release/fq-compressor compress input.fastq -o output.fqc \
    --threads 8 --memory-limit 4096
```

内存使用估算:
```
总内存 ≈ (blockSize × 50 bytes × maxInFlightBlocks) + 缓冲区 (128MB)
```

示例:
- `blockSize=100K`, `maxInFlightBlocks=8`: ~400MB + 128MB = **528MB**
- `blockSize=200K`, `maxInFlightBlocks=8`: ~800MB + 128MB = **928MB**

## 性能期望

基于理论分析和参考项目（pigz, pbzip2）的经验：

### 理想加速比

| 线程数 | 理论加速比 | 实际加速比* | CPU 利用率 |
|--------|------------|-------------|------------|
| 1 | 1.0x | 1.0x | 100% |
| 2 | 2.0x | ~1.7-1.8x | 85-90% |
| 4 | 4.0x | ~3.2-3.5x | 80-88% |
| 8 | 8.0x | ~6.0-7.0x | 75-88% |
| 16 | 16.0x | ~9-11x | 56-69% |

\* 实际加速比受限于 I/O 速度、数据特征和系统负载

### 影响因素

1. **I/O 速度**
   - HDD: 加速比受限（通常 < 4x）
   - SATA SSD: 中等加速比（4-6x）
   - NVMe SSD: 高加速比（6-10x）

2. **数据特征**
   - 高压缩率数据（压缩阶段占主导）→ 更高加速比
   - 低压缩率数据（I/O 占主导）→ 较低加速比

3. **CPU 架构**
   - 多核 CPU（Ryzen, Threadripper）→ 更好的并行性能
   - NUMA 系统（双路服务器）→ 需要调优（待实现）

## 故障排除

### 1. 性能不如预期

**问题**: 使用 8 线程，但加速比只有 2x

**可能原因**:
- I/O 瓶颈（使用 HDD 或网络文件系统）
- CPU 过载（其他程序占用资源）
- 内存不足（触发交换）

**解决方案**:
```bash
# 监控 I/O
iostat -x 1

# 监控 CPU
htop

# 检查内存
free -h

# 尝试减少线程数
./build/gcc-release/fq-compressor compress input.fastq -o output.fqc --threads 4
```

### 2. 内存不足

**问题**: 程序报错 "Out of memory" 或系统开始交换

**解决方案**:
```bash
# 方案 1: 减少块大小
./build/gcc-release/fq-compressor compress input.fastq -o output.fqc \
    --threads 4 --block-size 50000

# 方案 2: 设置内存限制
./build/gcc-release/fq-compressor compress input.fastq -o output.fqc \
    --threads 4 --memory-limit 2048

# 方案 3: 使用单线程模式
./build/gcc-release/fq-compressor compress input.fastq -o output.fqc --threads 1
```

### 3. 输出不一致

**问题**: 多线程和单线程输出的 .fqc 文件不同

**说明**: 这是**预期行为**。虽然解压后的 FASTQ 内容完全一致，但中间的压缩块顺序可能不同（因为并行处理）。

**验证**:
```bash
# 压缩（单线程）
./build/gcc-release/fq-compressor compress input.fastq -o single.fqc --threads 1

# 压缩（多线程）
./build/gcc-release/fq-compressor compress input.fastq -o multi.fqc --threads 4

# 解压并比较
./build/gcc-release/fq-compressor decompress single.fqc -o single.fq
./build/gcc-release/fq-compressor decompress multi.fqc -o multi.fq

# 应该完全一致
diff single.fq multi.fq
```

## 进度报告

并行模式下会每 2 秒报告一次进度：

```
[INFO] Progress: 25.3% (2530000 reads, 25 blocks, 45.2 MB/s)
[INFO] Progress: 50.1% (5010000 reads, 50 blocks, 47.8 MB/s)
[INFO] Progress: 75.7% (7570000 reads, 76 blocks, 46.5 MB/s)
[INFO] Progress: 100.0% (10000000 reads, 100 blocks, 47.1 MB/s)
```

禁用进度报告：
```bash
./build/gcc-release/fq-compressor compress input.fastq -o output.fqc --no-progress
```

## 技术细节

### TBB Pipeline 架构

```
压缩流水线:
  ┌──────────────────┐
  │ Stage 1 (Serial) │ 读取 FASTQ
  │   ReaderFilter   │
  └────────┬─────────┘
           │ ReadChunk
  ┌────────▼─────────┐
  │ Stage 2 (||)     │ 并行压缩
  │ CompressorFilter │
  └────────┬─────────┘
           │ CompressedBlock
  ┌────────▼─────────┐
  │ Stage 3 (Serial) │ 按序写入
  │  WriterFilter    │
  └──────────────────┘

解压流水线:
  ┌──────────────────┐
  │ Stage 1 (Serial) │ 读取 FQC
  │ FQCReaderFilter  │
  └────────┬─────────┘
           │ CompressedBlock
  ┌────────▼─────────┐
  │ Stage 2 (||)     │ 并行解压
  │DecompressorFilter│
  └────────┬─────────┘
           │ ReadChunk
  ┌────────▼─────────┐
  │ Stage 3 (Serial) │ 按序写入 FASTQ
  │FASTQWriterFilter │
  └──────────────────┘
```

### 线程安全保证

- **ReaderFilter**: 串行读取，无竞争
- **CompressorFilter**: 每个线程有独立的 compressor 实例
- **WriterFilter**: 串行写入，保证块顺序
- **Error Handling**: 使用 mutex 保护错误传播

### 内存管理

- **Backpressure Control**: 通过 `maxInFlightBlocks=8` 限制同时处理的块数
- **Zero-Copy**: 各阶段之间使用 `std::move` 传递数据
- **Memory Pooling**: 复用缓冲区（待实现）

## 未来改进

Phase 4 当前实现的局限和后续优化方向：

- [ ] **Paired-End 并行支持**: 当前 PE 模式仍使用串行处理
- [ ] **NUMA 优化**: 在多 socket 系统上绑定线程
- [ ] **异步 I/O**: 使用 io_uring 减少阻塞
- [ ] **动态调度**: 根据块大小动态调整并行度
- [ ] **内存池**: 复用缓冲区减少分配开销

详见: `/workspace/docs/phase4_tbb_parallel_implementation.md`

## 参考资料

- [Intel TBB 文档](https://spec.oneapi.io/versions/latest/elements/oneTBB/source/algorithms/functions/parallel_pipeline_func.html)
- [pigz - 并行 gzip](https://github.com/madler/pigz)
- [pbzip2 - 并行 bzip2](http://compression.ca/pbzip2/)
- Spring 论文中的并行策略

---
**版本**: Phase 4 (2026-01-28)
**作者**: fq-compressor 开发团队
