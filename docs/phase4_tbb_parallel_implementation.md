# Phase 4: TBB 并行优化实现报告

## 实施日期
2026-01-28

## 概述

本阶段实现了基于 Intel TBB `parallel_pipeline` 的并行压缩/解压流水线，以充分利用多核CPU资源加速处理。

## 实现内容

### 1. 压缩命令并行化 (compress_command.cpp)

#### 1.1 线程检测逻辑

在 `runCompression()` 方法中添加了线程检测：

```cpp
// 检测是否使用并行模式
if (options_.threads > 1 || (options_.threads == 0 && std::thread::hardware_concurrency() > 1)) {
    FQC_LOG_INFO("Using TBB parallel pipeline for compression");
    runCompressionParallel();
    return;
}

// 单线程模式
FQC_LOG_INFO("Using single-threaded compression");
// ... 原有单线程实现
```

**策略说明**:
- `options_.threads > 1`: 显式指定多线程
- `options_.threads == 0 && hardware_concurrency() > 1`: 自动检测，如果CPU有多核则启用并行
- `options_.threads == 1`: 强制单线程模式

#### 1.2 并行压缩实现 (runCompressionParallel)

新增方法使用 `pipeline::CompressionPipeline` 类封装TBB流水线：

```cpp
void CompressCommand::runCompressionParallel() {
    // 配置pipeline
    pipeline::CompressionPipelineConfig pipelineConfig;
    pipelineConfig.numThreads = options_.threads > 0 ? options_.threads : 0;  // 0 = auto
    pipelineConfig.blockSize = options_.blockSize;
    pipelineConfig.readLengthClass = options_.longReadMode;
    pipelineConfig.compressionLevel = options_.compressionLevel;
    pipelineConfig.enableReorder = options_.enableReordering && !options_.streamingMode;
    pipelineConfig.streamingMode = options_.streamingMode;
    pipelineConfig.qualityMode = /* 转换后的质量模式 */;

    // 进度回调
    if (options_.showProgress) {
        pipelineConfig.progressCallback = [](const ProgressInfo& info) {
            // 报告压缩进度
            return true;  // continue
        };
    }

    // 执行pipeline
    pipeline::CompressionPipeline pipeline(pipelineConfig);
    VoidResult result = pipeline.run(options_.inputPath, options_.outputPath);

    // 更新统计信息
    const auto& stats = pipeline.stats();
    stats_.totalReads = stats.totalReads;
    stats_.outputBytes = stats.outputBytes;
    // ...
}
```

### 2. 解压命令并行化 (decompress_command.cpp)

#### 2.1 线程检测逻辑

与压缩相同的策略：

```cpp
if (options_.threads > 1 || (options_.threads == 0 && std::thread::hardware_concurrency() > 1)) {
    FQC_LOG_INFO("Using TBB parallel pipeline for decompression");
    runDecompressionParallel();
    return;
}

FQC_LOG_INFO("Using single-threaded decompression");
// ... 原有实现
```

#### 2.2 并行解压实现 (runDecompressionParallel)

```cpp
void DecompressCommand::runDecompressionParallel() {
    // 配置pipeline
    pipeline::DecompressionPipelineConfig pipelineConfig;
    pipelineConfig.numThreads = options_.threads > 0 ? options_.threads : 0;
    pipelineConfig.rangeStart = options_.range ? options_.range->start : 0;
    pipelineConfig.rangeEnd = options_.range ? options_.range->end : 0;
    pipelineConfig.originalOrder = options_.originalOrder;
    pipelineConfig.headerOnly = options_.headerOnly;
    pipelineConfig.verifyChecksums = options_.verifyChecksums;
    pipelineConfig.skipCorrupted = options_.skipCorrupted;

    // 进度回调
    if (options_.showProgress) {
        pipelineConfig.progressCallback = [](const ProgressInfo& info) {
            // 报告解压进度
            return true;
        };
    }

    // 执行pipeline
    pipeline::DecompressionPipeline pipeline(pipelineConfig);

    VoidResult result;
    if (options_.splitPairedEnd) {
        result = pipeline.runPaired(inputPath, output1Path, output2Path);
    } else {
        result = pipeline.run(inputPath, outputPath);
    }

    // 更新统计
    const auto& stats = pipeline.stats();
    stats_.totalReads = stats.totalReads;
    // ...
}
```

### 3. Pipeline 三阶段架构

TBB 流水线实现在 `/workspace/src/pipeline/pipeline.cpp` 中，采用经典的三阶段设计：

#### 压缩流水线

```
Stage 1 (Serial, In-order): ReaderFilter
  ├─ 读取 FASTQ 文件
  ├─ 解析 FASTQ 记录
  └─ 生成 ReadChunk (blockSize 条reads)

Stage 2 (Parallel): CompressorFilter
  ├─ 多线程并行压缩多个 ReadChunk
  ├─ ID 压缩 (Delta + Tokenization)
  ├─ Sequence 压缩 (ABC/Zstd)
  ├─ Quality 压缩 (SCM/Illumina8)
  └─ 生成 CompressedBlock

Stage 3 (Serial, In-order): WriterFilter
  ├─ 按序写入 CompressedBlock
  ├─ 写入 Block Header + Payload
  └─ 更新 Block Index
```

#### 解压流水线

```
Stage 1 (Serial, In-order): FQCReaderFilter
  ├─ 读取 FQC 文件
  ├─ 解析 Block Header
  └─ 读取 CompressedBlock

Stage 2 (Parallel): DecompressorFilter
  ├─ 多线程并行解压多个 CompressedBlock
  ├─ ID 解压
  ├─ Sequence 解压
  ├─ Quality 解压
  └─ 生成 ReadChunk

Stage 3 (Serial, In-order): FASTQWriterFilter
  ├─ 按序写入 FASTQ 记录
  └─ 更新统计信息
```

## 关键技术点

### 1. Backpressure 控制

通过 `maxInFlightBlocks` 参数限制内存使用：

```cpp
tbb::parallel_pipeline(
    config_.maxInFlightBlocks,  // 默认 8 个 block
    // ... filters
);
```

这确保了在并行处理时，不会无限制地缓存未处理的块。

### 2. 线程安全

- **CompressorNode**: 每个线程有独立的 compressor 实例（通过 round-robin 分配）
- **WriterNode**: 串行写入，确保块按序输出
- **ErrorHandler**: 使用 `std::mutex` 保护错误传播

```cpp
// 线程局部 compressor 池
std::vector<std::unique_ptr<CompressorNode>> compressors;
for (size_t i = 0; i < numThreads; ++i) {
    compressors.push_back(std::make_unique<CompressorNode>(config));
}

// Round-robin 分配
size_t idx = compressorIndex.fetch_add(1) % compressors.size();
auto result = compressors[idx]->compress(chunk);
```

### 3. 进度报告

使用原子变量跟踪进度，每 2 秒报告一次：

```cpp
std::atomic<uint64_t> readsProcessed{0};
std::atomic<uint32_t> blocksProcessed{0};

// Stage 3 writer 中更新
readsProcessed.fetch_add(readCount);
blocksProcessed.fetch_add(1);

// 每 progressIntervalMs (2000ms) 触发回调
if (config_.progressCallback && elapsed >= 2000) {
    ProgressInfo info;
    info.readsProcessed = readsProcessed.load();
    info.currentBlock = blocksProcessed.load();
    config_.progressCallback(info);
}
```

### 4. 错误传播

Pipeline 中任何阶段的错误会被捕获并停止整个流水线：

```cpp
std::atomic<bool> readerError{false};
std::atomic<bool> writerError{false};
std::optional<Error> pipelineError;
std::mutex errorMutex;

// 在任意 filter 中捕获错误
if (!result) {
    std::lock_guard lock(errorMutex);
    pipelineError = result.error();
    readerError.store(true);  // 通知其他 stage 停止
    fc.stop();  // 停止 pipeline
}
```

## 性能优化要点

### 1. 零拷贝传递

Pipeline 各阶段之间使用 `std::move` 传递数据，避免拷贝：

```cpp
// Stage 1 -> Stage 2
return std::move(chunk);  // ReadChunk

// Stage 2 -> Stage 3
return std::move(block);  // CompressedBlock
```

### 2. 内存预算

通过配置参数控制内存使用：

- `blockSize`: 每个 chunk 的 read 数量
- `maxInFlightBlocks`: 最多同时处理的 block 数
- `memoryLimitMB`: 总内存限制

估算公式（在 `pipeline.cpp::estimateMemoryUsage` 中）:

```cpp
// Phase 1: Reordering (if enabled)
phase1Memory = totalReads * 24 bytes  // minimizer index + reorder map

// Phase 2: Block processing
phase2Memory = blockSize * 50 bytes * maxInFlightBlocks

// Buffers
bufferMemory = inputBufferSize + outputBufferSize

totalMemory = phase1Memory + phase2Memory + bufferMemory
```

### 3. TBB Task Arena

限制 TBB 使用的线程数，避免过度竞争：

```cpp
tbb::task_arena arena(static_cast<int>(config_.effectiveThreads()));

arena.execute([&] {
    tbb::parallel_pipeline(/* ... */);
});
```

## 使用方式

### 命令行参数

**压缩**:
```bash
# 自动检测线程数
fq-compressor compress input.fq -o output.fqc

# 指定 4 线程
fq-compressor compress input.fq -o output.fqc --threads 4

# 强制单线程
fq-compressor compress input.fq -o output.fqc --threads 1
```

**解压**:
```bash
# 自动检测线程数
fq-compressor decompress input.fqc -o output.fq

# 指定 8 线程
fq-compressor decompress input.fqc -o output.fq --threads 8

# 强制单线程
fq-compressor decompress input.fqc -o output.fq --threads 1
```

### 性能期望

根据理论分析和参考项目（pigz, pbzip2）的经验：

| 线程数 | 预期加速比 | 说明 |
|--------|------------|------|
| 1 | 1.0x | 基准（单线程） |
| 2 | ~1.8x | 串行阶段开销约 10% |
| 4 | ~3.5x | 理想情况下接近 4x |
| 8 | ~6.5x | 受限于 I/O 和串行阶段 |
| 16+ | ~8-10x | 收益递减，建议配合 NVMe SSD |

**注意**: 实际加速比依赖于：
- CPU 核心数和频率
- I/O 速度（HDD < SATA SSD < NVMe SSD）
- 数据特征（压缩率、read length）
- 系统负载

## 验证测试

### 单元测试计划

需要在 `/workspace/tests/pipeline/` 中添加：

1. **并行正确性测试**
   - 单线程 vs 多线程输出一致性
   - 验证块顺序保持不变

2. **性能基准测试**
   - 测试 1/2/4/8 线程的吞吐量
   - 记录加速比曲线

3. **错误处理测试**
   - 中途取消 pipeline
   - 模拟 I/O 错误
   - 验证错误传播

### 手动测试命令

```bash
# 测试小文件 (100K reads)
time fq-compressor compress small.fq -o small.fqc --threads 1
time fq-compressor compress small.fq -o small_mt.fqc --threads 4
diff <(fq-compressor decompress small.fqc -o -) \
     <(fq-compressor decompress small_mt.fqc -o -)

# 测试大文件 (10M reads)
time fq-compressor compress large.fq -o large.fqc --threads 1
time fq-compressor compress large.fq -o large_mt.fqc --threads 8

# 性能分析
perf stat -e cycles,instructions,cache-misses \
    fq-compressor compress input.fq -o output.fqc --threads 8
```

## 遗留问题与改进方向

### 1. 当前已知限制

- [ ] **Paired-end 并行支持不完整**: 当前 paired-end 模式仍使用串行处理（见 `runPaired` 实现）
- [ ] **进度报告精度**: 流式模式下无法获取总 read 数，进度百分比不准确
- [ ] **内存估算粗糙**: `estimateMemoryUsage` 使用固定系数，未考虑长读数据

### 2. 性能优化建议

- [ ] **I/O 优化**: 考虑使用 `io_uring` 或异步 I/O 减少阻塞
- [ ] **NUMA 感知**: 在多 socket 系统上绑定线程到特定 NUMA 节点
- [ ] **Dynamic Scheduling**: 根据 block 大小动态调整并行度

### 3. 功能增强

- [ ] **Cancellation Token**: 支持外部取消信号（如 Ctrl+C）
- [ ] **Pipeline Profiling**: 记录各 stage 的耗时，识别瓶颈
- [ ] **Adaptive Backpressure**: 根据内存压力动态调整 `maxInFlightBlocks`

## 参考资料

- [Intel TBB parallel_pipeline 文档](https://spec.oneapi.io/versions/latest/elements/oneTBB/source/algorithms/functions/parallel_pipeline_func.html)
- [pigz 并行实现](https://github.com/madler/pigz) - gzip 的并行版本
- [pbzip2 设计](http://compression.ca/pbzip2/) - bzip2 的并行版本
- Spring 论文中的并行策略分析

## 总结

本阶段成功实现了基于 TBB 的三阶段并行流水线，通过合理的线程调度和内存管理，在保持输出正确性的前提下，预期可实现 4-8x 的加速比（取决于硬件配置）。

**下一步工作**:
- 编写完整的性能基准测试
- 优化 paired-end 并行支持
- 验证不同数据集（short/medium/long reads）的性能表现

---
**作者**: Claude (Anthropic AI)
**实施日期**: 2026-01-28
**审核状态**: 待测试验证
