# fq-compressor 架构摩擦点分析报告

## 1. 模块深度分析

### 深层模块 (高杠杆)
- **block_compressor** (header: 448行, impl: 1667行)
  - 接口: compress/decompress + 配置
  - 实现: ABC算法 + delta编码 + consensus构建
  - 杠杆率高: 一个接口隐藏了复杂的consensus构建、delta编码、噪声压缩逻辑
  
- **pipeline** (header: 584行, impl: 1269行)
  - 接口: run/runPaired + 配置
  - 实现: TBB并行管道协调、背压控制、统计收集
  - 杠杆率中: 封装了并行执行细节,但大部分逻辑在各个Node中

### 浅层模块 (interface ≈ implementation)
- **pipeline_node** (header: 681行, impl: 114行)
  - 大部分代码是配置验证和简单的BackpressureController
  - 实际节点实现在单独文件中(reader_node.cpp等)
  - **删除测试**: 删除这个文件只会分散验证逻辑到各节点实现中

- **stream_factory** (header: 352行, impl: 226行)
  - 接口: createInputStream/createOutputStream
  - 实现: 文件打开+压缩检测
  - 杠杆率低: 几乎是直接透传到compressed_stream

### 透传层
- **CompressorNode** (impl: 482行)
  - 主要工作: 将ReadChunk拆分为string_view vectors
  - 调用: compressIds/compressSequences/compressQualities
  - **删除测试**: 如果删除,调用者需要自己处理数据转换,但没有逻辑消失

- **WriterNode/ReaderNode**
  - 主要是配置传递和状态管理
  - 实际I/O在fqc_writer/fqc_reader中

## 2. 接缝(Seam)识别

### 真实接缝 (有多个adapter)
- **StreamFactory** (FileStreamFactory, MemoryStreamFactory)
  - 文件: include/fqc/io/stream_factory.h
  - 用途: I/O依赖注入,支持测试
  - **问题**: 测试中实际使用MemoryStreamFactory的地方很少

- **BlockCompressor接口**
  - 理论上可替换不同的压缩策略
  - **问题**: 没有其他实现,所有路径都使用BlockCompressor

### 假设接缝 (只有一个adapter)
- **GlobalAnalyzer** - 只有一个实现
- **QualityCompressor** - 只有一个实现
- **IDCompressor** - 只有一个实现
- **PEOptimizer** - 只有一个实现

### 耦合泄漏
1. **CompressCommand → 多个Config转换**
   ```cpp
   // 在setupCompressionParams中
   BlockCompressorConfig compressorConfig;
   CompressionPipelineConfig pipelineConfig;
   // 手动复制字段: level, threads, qualityMode, idMode...
   ```
   - 每个配置对象有不同但重叠的字段集
   - 字段变更需要修改N个地方的转换代码

2. **CompressorNode → BlockCompressor内部细节**
   ```cpp
   // compressor_node.cpp 需要知道:
   - 如何拆分ReadChunk到string_view vectors
   - 如何调用ID/Quality压缩器
   - 如何计算checksum
   ```
   - CompressorNode变成了BlockCompressor的一部分实现

3. **PipelineNode.h 包含所有节点的实现细节**
   - ReaderNode, CompressorNode, WriterNode等都在同一个头文件
   - 修改任何节点都需要重新编译所有节点

## 3. 测试表面分析

### 容易测试 (通过interface)
- **FastqParser** (使用StreamFactory注入)
  - 文件: tests/io/fastq_parser_property_test.cpp
  - 可以用MemoryStreamFactory测试解析逻辑
  - **问题**: 实际测试没有使用MemoryStreamFactory,而是直接用string构造

- **BlockCompressor**
  - 文件: tests/algo/block_compressor_regression_test.cpp
  - 直接构造ReadRecords测试压缩/解压

### 测试困难 (实现细节泄漏)
- **CompressionPipeline**
  - 测试: tests/pipeline/pipeline_property_test.cpp
  - 问题: 需要完整文件系统交互
  - 无法mock: ReaderNode, CompressorNode, WriterNode
  - 结果: 测试很慢,需要创建临时文件

- **CompressCommand**
  - 直接操作文件系统和完整pipeline
  - 无法单独测试配置转换逻辑
  - 测试: tests/commands/archive_regression_test.cpp (端到端测试)

### 纯函数提取的陷阱
```cpp
// block_compressor.cpp
DeltaEncodedRead computeDelta(...)  // 纯函数,容易测试
std::string reconstructFromDelta(...)  // 纯函数,容易测试

// 但是真正的bug隐藏在:
void BlockCompressorImpl::buildContigs(...) {
    // 复杂的read分配和consensus更新逻辑
    // 使用了member variable: contigs_
    // 依赖: config_.maxShift, config_.consensusHammingThreshold
}
```
- 测试了computeDelta,但bug可能在buildContigs的调用方式中

## 4. 代码导航摩擦

### 概念跳转次数
理解"如何压缩一个FASTQ文件"需要跳转:

1. `main()` → `CompressCommand::execute()`
2. `CompressCommand` → `detectReadLengthClass()` → `GlobalAnalyzer`
3. `CompressCommand` → `runCompressionParallel()` → `CompressionPipeline`
4. `CompressionPipeline` → `ReaderNode::readChunk()`
5. `ReaderNode` → `FastqParser` → `StreamFactory`
6. `CompressionPipeline` → `CompressorNode::compress()`
7. `CompressorNode` → `BlockCompressor::compress()`
8. `BlockCompressor` → `buildContigs()` + `compressSequencesABC()`
9. `BlockCompressor` → `IDCompressor` + `QualityCompressor`
10. `CompressionPipeline` → `WriterNode::writeBlock()`
11. `WriterNode` → `FQCWriter`

**跳转次数**: ~11个文件,跨4个目录层级

### 命名不一致

**ReadRecord vs FastqRecord**
```cpp
// include/fqc/common/types.h
struct ReadRecord { ... };

// include/fqc/io/fastq_parser.h
struct FastqRecord { ... };
```
- 两个几乎相同的类型
- FastqParser返回FastqRecord
- BlockCompressor需要ReadRecord
- 需要转换(虽然有隐式转换)

**Config vs Options**
```cpp
struct CompressionOptions { ... };     // common/types.h
struct CompressOptions { ... };        // commands/compress_command.h
struct CompressionPipelineConfig { ... };  // pipeline/pipeline.h
struct BlockCompressorConfig { ... };  // algo/block_compressor.h
```
- 4个不同但重叠的配置对象
- 命名混乱: Options vs Config

### 不必要的抽象层

**CompressorNode作为单独的类**
```cpp
class CompressorNode {
    Result<CompressedBlock> compress(ReadChunk chunk);
private:
    std::unique_ptr<CompressorNodeImpl> impl_;
};
```
- 482行实现
- 主要工作: 数据转换和调用BlockCompressor
- **问题**: 这个抽象没有提供价值
  - 不能单独测试(因为BlockCompressor不是接口)
  - 不能替换实现(只有一个实现)
  - 增加了一层间接,但没有隐藏复杂性

## 5. 具体摩擦点示例

### 配置传递链
```
CompressOptions (CLI层)
  ↓ 手动字段复制
CompressionPipelineConfig (Pipeline层)
  ↓ 手动字段复制
BlockCompressorConfig (算法层)
  ↓ 手动字段复制
CompressorNodeConfig (节点层)
```

### 数据类型转换链
```
FastqRecord (Parser输出)
  ↓ 隐式转换
ReadRecord (通用类型)
  ↓ 拆分为vectors
string_view vectors (CompressorNode内部)
  ↓ 传递给压缩器
compressed bytes
```

### 测试需要跳过的层级
```
想测试: ABC算法的consensus构建
需要: 
  1. 构造ReadRecords
  2. 创建BlockCompressorConfig
  3. 创建BlockCompressor
  4. 调用compress()

无法:
  - 直接测试buildContigs()
  - 直接测试consensus更新逻辑
  - 因为这些都是BlockCompressorImpl的private方法
```

## 6. 重点问题总结

### algo/ 目录
- **block_compressor**: 高复杂度但深层,杠杆率高
- **quality_compressor, id_compressor**: Pimpl模式但只有一个实现
- **global_analyzer**: 假设接缝,无其他实现

### format/ 目录
- **fqc_format.h**: 纯数据结构,无实现,优秀
- **fqc_reader, fqc_writer**: 实现复杂但接口清晰

### io/ 目录
- **StreamFactory**: 真实接缝但测试未使用
- **FastqParser**: 有FastqRecord/ReadRecord重复

### pipeline/ 目录
- **pipeline_node.h**: 所有节点混在一起
- **CompressorNode**: 透传层,删除测试失败
- **配置爆炸**: N个Config结构体

### commands/ 目录
- **CompressCommand**: 大量配置转换代码
- **测试**: 主要是端到端测试,慢


## 7. 具体代码示例

### 示例1: 配置对象爆炸

**问题**: 4个不同但重叠的配置对象

```cpp
// include/fqc/common/types.h:515
struct CompressionOptions {
    CompressionLevel level = kDefaultCompressionLevel;
    QualityMode qualityMode = QualityMode::kLossless;
    IDMode idMode = IDMode::kExact;
    bool enableReorder = true;
    bool saveReorderMap = true;
    bool streamingMode = false;
    std::size_t blockSize = kDefaultBlockSizeShort;
    std::size_t memoryLimitMB = kDefaultMemoryLimitMB;
    std::size_t threads = 0;
    PELayout peLayout = PELayout::kInterleaved;
    std::optional<ReadLengthClass> readLengthClass;
    std::size_t maxBlockBases = kDefaultMaxBlockBasesLong;
};

// include/fqc/commands/compress_command.h:41
struct CompressOptions {
    std::filesystem::path inputPath;
    std::filesystem::path input2Path;
    std::filesystem::path outputPath;
    int compressionLevel = 6;
    int threads = 0;
    std::size_t memoryLimitMb = 0;
    bool enableReordering = true;
    bool streamingMode = false;
    QualityMode qualityMode = QualityMode::kLossless;
    IDMode idMode = IDMode::kExact;
    ReadLengthClass longReadMode = ReadLengthClass::kShort;
    bool autoDetectLongRead = true;
    bool scanAllLengths = false;
    std::size_t maxBlockBases = 0;
    bool paired = false;
    bool interleaved = false;
    PELayout peLayout = PELayout::kInterleaved;
    std::size_t blockSize = 100000;
    bool blockSizeExplicit = false;
    bool saveReorderMap = true;
    ChecksumType checksumType = ChecksumType::kXxHash64;
    bool forceOverwrite = false;
    bool showProgress = true;
    bool validateInput = true;
    bool collectStats = true;
};

// include/fqc/pipeline/pipeline.h:296
struct CompressionPipelineConfig {
    std::size_t numThreads = 0;
    std::size_t maxInFlightBlocks = kDefaultMaxInFlightBlocks;
    std::size_t inputBufferSize = kDefaultInputBufferSize;
    std::size_t outputBufferSize = kDefaultOutputBufferSize;
    std::size_t memoryLimitMB = kDefaultMemoryLimitMB;
    std::size_t blockSize = kDefaultBlockSizeShort;
    ReadLengthClass readLengthClass = ReadLengthClass::kShort;
    QualityMode qualityMode = QualityMode::kLossless;
    IDMode idMode = IDMode::kExact;
    CompressionLevel compressionLevel = kDefaultCompressionLevel;
    bool enableReorder = true;
    bool saveReorderMap = true;
    bool streamingMode = false;
    ProgressCallback progressCallback;
    std::uint32_t progressIntervalMs = 500;
};

// include/fqc/algo/block_compressor.h:161
struct BlockCompressorConfig {
    ReadLengthClass readLengthClass = ReadLengthClass::kShort;
    QualityMode qualityMode = QualityMode::kLossless;
    IDMode idMode = IDMode::kExact;
    CompressionLevel compressionLevel = kDefaultCompressionLevel;
    int zstdLevel = kDefaultZstdLevel;
    std::size_t numThreads = 0;
    std::size_t consensusMinReads = kDefaultConsensusMinReads;
    std::size_t maxShift = kDefaultMaxShift;
    std::size_t consensusHammingThreshold = kDefaultConsensusHammingThreshold;
    std::function<void(double)> progressCallback;
};
```

**摩擦点**:
- `compressionLevel` 在3个结构中都有
- `qualityMode`, `idMode` 在3个结构中都有
- `threads` 在2个结构中(名字不同: threads vs numThreads)
- 每次添加新选项需要修改4个地方

### 示例2: 数据类型重复

```cpp
// include/fqc/common/types.h:340
struct ReadRecord {
    std::string id;
    std::string comment;
    std::string sequence;
    std::string quality;
    // ... methods
};

// include/fqc/io/fastq_parser.h:63
struct FastqRecord {
    std::string id;
    std::string comment;
    std::string sequence;
    std::string quality;
    // ... methods
};
```

**摩擦点**: 
- 两个几乎相同的结构
- FastqParser返回FastqRecord
- BlockCompressor需要ReadRecord
- 有隐式转换但增加了认知负担

### 示例3: 透传层 - CompressorNode

**文件**: src/pipeline/compressor_node.cpp (482行)

```cpp
Result<CompressedBlock> CompressorNodeImpl::compress(ReadChunk chunk) {
    // ... 状态管理
    
    // 主要工作: 数据转换
    std::vector<std::string_view> ids;
    std::vector<std::string_view> comments;
    std::vector<std::string_view> sequences;
    std::vector<std::string_view> qualities;
    std::vector<std::uint32_t> lengths;
    
    for (const auto& read : chunk.reads) {
        ids.push_back(read.id);
        comments.push_back(read.comment);
        sequences.push_back(read.sequence);
        qualities.push_back(read.quality);
        if (!uniformLength) {
            lengths.push_back(read.sequence.size());
        }
    }
    
    // 调用真正的压缩器
    auto idResult = compressIds(ids, comments);
    auto seqResult = compressSequences(sequences);
    auto qualResult = compressQualities(qualities, sequences);
    
    // ... 更多转换
}
```

**删除测试**:
- 如果删除CompressorNode,pipeline需要自己做数据转换
- 但没有逻辑消失,只是转移了转换代码
- **结论**: 透传层,没有提供抽象价值

### 示例4: Pimpl模式的滥用

**问题**: 所有主要类都用Pimpl,但只有一个实现

```cpp
// include/fqc/algo/block_compressor.h
class BlockCompressor {
public:
    explicit BlockCompressor(BlockCompressorConfig config = {});
    ~BlockCompressor();
    
    Result<CompressedBlockData> compress(...);
    Result<DecompressedBlockData> decompress(...);
    
private:
    std::unique_ptr<BlockCompressorImpl> impl_;  // Pimpl
};

// 类似地:
class QualityCompressor { std::unique_ptr<QualityCompressorImpl> impl_; };
class IDCompressor { std::unique_ptr<IDCompressorImpl> impl_; };
class GlobalAnalyzer { std::unique_ptr<GlobalAnalyzerImpl> impl_; };
class CompressionPipeline { std::unique_ptr<CompressionPipelineImpl> impl_; };
class ReaderNode { std::unique_ptr<ReaderNodeImpl> impl_; };
// ... 等等
```

**摩擦点**:
- 增加了间接跳转
- 没有实际的实现替换
- 编译防火墙的价值有限(头文件已经暴露了所有依赖)
- 测试时无法mock这些类

### 示例5: 测试困难 - Pipeline测试

**文件**: tests/pipeline/pipeline_property_test.cpp

```cpp
RC_GTEST_PROP(PipelinePropertyTest, RoundTripConsistency,
              (const std::vector<ReadRecord>& records)) {
    // 1. 创建临时文件
    auto tempDir = std::filesystem::temp_directory_path();
    auto inputPath = tempDir / "test_input.fastq";
    auto outputPath = tempDir / "test_output.fqc";
    auto decompressedPath = tempDir / "test_decompressed.fastq";
    
    // 2. 写入FASTQ文件
    {
        std::ofstream ofs(inputPath);
        for (const auto& record : records) {
            ofs << '@' << record.id << '\n'
                << record.sequence << '\n'
                << "+\n"
                << record.quality << '\n';
        }
    }
    
    // 3. 运行完整pipeline
    CompressionPipeline pipeline(config);
    auto result = pipeline.run(inputPath, outputPath);
    RC_ASSERT(result.has_value());
    
    // 4. 运行解压pipeline
    DecompressionPipeline decompPipeline(decompConfig);
    auto decompResult = decompPipeline.run(outputPath, decompressedPath);
    
    // 5. 读取并验证
    // ...
    
    // 6. 清理临时文件
    std::filesystem::remove(inputPath);
    std::filesystem::remove(outputPath);
    std::filesystem::remove(decompressedPath);
}
```

**摩擦点**:
- 无法单独测试pipeline协调逻辑
- 必须涉及文件系统
- 测试慢(需要创建文件、运行完整pipeline)
- 无法测试错误路径(如ReaderNode失败)

### 示例6: 接缝未被使用

**StreamFactory的真实接缝**

```cpp
// include/fqc/io/stream_factory.h
class StreamFactory {
public:
    virtual std::unique_ptr<std::istream> createInputStream(
        const std::filesystem::path& path) = 0;
    virtual std::unique_ptr<std::ostream> createOutputStream(
        const std::filesystem::path& path, 
        CompressionFormat format = CompressionFormat::kNone) = 0;
};

class FileStreamFactory : public StreamFactory { ... };
class MemoryStreamFactory : public StreamFactory { ... };
```

**实际测试中**:

```cpp
// tests/io/fastq_parser_property_test.cpp
RC_GTEST_PROP(FastqParserPropertyTest, ParseRoundTrip,
              (const std::vector<FastqRecord>& records)) {
    // 直接构造字符串,不使用MemoryStreamFactory
    std::string fastqData;
    for (const auto& record : records) {
        fastqData += formatFastqRecord(record);
    }
    
    // 直接从string构造istream
    auto stream = std::make_unique<std::istringstream>(fastqData);
    FastqParser parser(std::move(stream), options);
    // ...
}
```

**摩擦点**:
- MemoryStreamFactory从未被使用
- FastqParser有两个构造函数(一个接受factory,一个接受stream)
- 增加了API复杂度但没有提供价值

### 示例7: 概念跳转 - 完整路径

**问题**: 理解"压缩一个文件"需要追踪11个文件

```
main.cpp
  ↓ CompressCommand::execute()
compress_command.cpp:76
  ↓ detectReadLengthClass()
compress_command.cpp:201
  ↓ GlobalAnalyzer::analyze()
global_analyzer.cpp:200+
  ↓ runCompressionParallel()
compress_command.cpp:294
  ↓ CompressionPipeline::run()
pipeline.cpp:183
  ↓ ReaderNode::readChunk()
reader_node.cpp:55
  ↓ FastqParser::readChunk()
fastq_parser.cpp:290
  ↓ CompressorNode::compress()
compressor_node.cpp:33
  ↓ BlockCompressor::compress()
block_compressor.cpp:700+
  ↓ buildContigs() + compressSequencesABC()
block_compressor.cpp:651, 900+
  ↓ WriterNode::writeBlock()
writer_node.cpp:45
  ↓ FQCWriter::writeBlock()
fqc_writer.cpp:150+
```

**每个文件需要理解**:
1. main.cpp - 入口
2. compress_command.cpp - 配置转换
3. global_analyzer.cpp - 全局分析
4. pipeline.cpp - 并行协调
5. reader_node.cpp - 读取节点
6. fastq_parser.cpp - FASTQ解析
7. compressor_node.cpp - 压缩节点(转换)
8. block_compressor.cpp - 实际压缩
9. writer_node.cpp - 写入节点
10. fqc_writer.cpp - FQC格式写入
11. id_compressor.cpp, quality_compressor.cpp - 辅助压缩

**总代码行数**: ~6000行 (仅这些文件)
**跳转次数**: ~11次
**目录层级**: 4层 (commands, pipeline, algo, io, format)

