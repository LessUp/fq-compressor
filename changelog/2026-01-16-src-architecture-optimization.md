# src/ 架构优化与重构

**日期**: 2026-01-16  
**范围**: `src/pipeline/`, `src/io/`, `src/main.cpp`, `src/CMakeLists.txt`

## 变更摘要

本次对 `src/` 目录进行了系统性的性能优化和架构重构，涵盖 8 项改进。

---

## P0 — 性能关键修复

### P0-2: 修复 Compressor round-robin 数据竞争
- **文件**: `src/pipeline/pipeline.cpp`
- **问题**: `compressors.size()` 按 `effectiveThreads()` 创建，但 TBB `parallel_pipeline` 最多有 `maxInFlightBlocks` 个并发 token，round-robin 可能导致两个 token 共享同一个非线程安全的 compressor
- **修复**: 压缩和解压管道均改为创建 `maxInFlightBlocks` 个 compressor/decompressor 实例

### P0-1: PE 压缩并行化
- **文件**: `src/pipeline/pipeline.cpp`
- **问题**: `CompressionPipelineImpl::runPaired()` 使用串行循环，未利用 TBB 并行化，PE 数据（生产主流场景）只能单核压缩
- **修复**: 替换为与 SE `run()` 相同的 TBB `parallel_pipeline` 三阶段模型（Reader → Compressor → Writer）

### P0-4: 减少 CompressorNode sequence 冗余拷贝
- **文件**: `src/pipeline/compressor_node.cpp` (原 `pipeline_node.cpp`)
- **问题**: `compressSequences()` 将所有 `string_view` 深拷贝为 `ReadRecord`（含 placeholder quality），双倍内存开销
- **修复**: 改用零拷贝 `ReadRecordView`，复用 `seq` 作为 quality placeholder

### P0-3: DecompressorNode 缓存 BlockCompressor 复用
- **文件**: `src/pipeline/decompressor_node.cpp` (原 `pipeline_node.cpp`)
- **问题**: 每个 block 解压都创建/销毁一个 `BlockCompressor`（含 Zstd context）
- **修复**: 延迟初始化 `cachedCompressor_` 成员，首次 decompress 时创建，后续复用

### P0-5: AsyncIO 接入 pipeline ReaderNode
- **文件**: `include/fqc/io/async_io.h`, `src/io/async_io.cpp`, `src/pipeline/reader_node.cpp`
- **问题**: `AsyncReader`/`AsyncWriter` 已实现完整预读/回写机制，但 pipeline 的 `ReaderNode` 直接用同步 `FastqParser`
- **修复**: 
  - 新增 `AsyncStreamBuf`（自定义 `std::streambuf` 适配器），通过 `underflow()` 从 `AsyncReader` 获取预读数据
  - 新增 `createAsyncInputStream()` 工厂函数
  - `ReaderNodeImpl::open()` 对非压缩文件自动启用 async prefetch

## P1 — 数据完整性与架构

### P1-9: 用 xxHash 替换 placeholder checksum
- **文件**: `src/pipeline/compressor_node.cpp` (原 `pipeline_node.cpp`)
- **问题**: `calculateBlockChecksum()` 使用 `hash * 31 + c` 简单哈希，完全不可靠
- **修复**: 替换为 `XXH3_64bits` streaming API（`XXH3_64bits_reset/update/digest`）

### P1-6: 拆分 pipeline_node.cpp 为 6 个独立文件
- **新增文件**:
  - `src/pipeline/reader_node.cpp` — ReaderNodeImpl + ReaderNode 公共接口
  - `src/pipeline/compressor_node.cpp` — CompressorNodeImpl + CompressorNode 公共接口
  - `src/pipeline/writer_node.cpp` — WriterNodeImpl + WriterNode 公共接口
  - `src/pipeline/fqc_reader_node.cpp` — FQCReaderNodeImpl + FQCReaderNode 公共接口
  - `src/pipeline/decompressor_node.cpp` — DecompressorNodeImpl + DecompressorNode 公共接口
  - `src/pipeline/fastq_writer_node.cpp` — FASTQWriterNodeImpl + FASTQWriterNode 公共接口
- **修改文件**: `src/pipeline/pipeline_node.cpp` 瘦身为仅配置验证 + BackpressureController
- **修改文件**: `src/CMakeLists.txt` 添加 6 个新源文件

### P1-7: main.cpp 重构 — 消除全局状态
- **文件**: `src/main.cpp`
- **问题**: 5 个全局可变变量（`gOptions`, `gCompressOpts` 等）；`runXxx()` 在 main.cpp 定义但声明在 `fqc::commands` namespace
- **修复**:
  - 所有选项改为 `main()` 内局部变量
  - `setupXxxCommand()` 改为接受引用参数
  - `runXxx()` 内联到 `main()` 的 dispatch 逻辑中，消除 `fqc::commands` 分裂定义

---

## 后续待办（Deferred）

- **P1-8**: 配置结构体合并为 2 层（CLI Options → Internal Config）
- **P1-10**: DNA 查找表集中化到 `common/dna_encoding.h`
