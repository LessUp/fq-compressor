# fq-compressor 详细问题清单与修复方案

> 文档版本: 2.0
> 更新日期: 2026-01-29
> 基于: COMPREHENSIVE_DIAGNOSIS.md

---

## P0 级别问题（阻塞MVP）

### ISSUE-P0-001: IDCompressor Zstd压缩层未实现

**位置**: `/workspace/src/algo/id_compressor.cpp:956-977`

**问题描述**:
IDCompressor的Zstd压缩和解压方法为占位符实现，直接返回未压缩数据。这导致ID流的压缩比为1.0，完全浪费了Delta编码的收益。

**代码片段**:
```cpp
std::vector<std::uint8_t> IDCompressorImpl::compressWithZstd(
    std::span<const std::uint8_t> data) {
    // TODO: Integrate actual Zstd compression
    // For now, return data as-is (no compression)
    return std::vector<std::uint8_t>(data.begin(), data.end());
}

std::expected<std::vector<std::uint8_t>, std::string>
IDCompressorImpl::decompressWithZstd(std::span<const std::uint8_t> data) {
    // TODO: Integrate actual Zstd decompression
    // For now, return data as-is (no decompression)
    return std::vector<std::uint8_t>(data.begin(), data.end());
}
```

**影响分析**:
- ID流压缩比: 1.0 (预期: 5-10x)
- 整体压缩比损失: 15-20%
- Delta+Varint编码的收益完全丢失
- 功能正确性: ✅ 不影响（往返一致）
- 性能目标: ❌ 无法达标

**根本原因**:
Phase 2实现时留下的TODO，计划在Phase 5集成Zstd库。

**修复方案**:

1. **包含Zstd头文件**:
```cpp
#include <zstd.h>
```

2. **实现compressWithZstd**:
```cpp
std::vector<std::uint8_t> IDCompressorImpl::compressWithZstd(
    std::span<const std::uint8_t> data) {
    if (data.empty()) {
        return {};
    }

    // 预估压缩后大小
    size_t const cBuffSize = ZSTD_compressBound(data.size());
    std::vector<std::uint8_t> compressed(cBuffSize);

    // 压缩（使用level 3，平衡速度和压缩比）
    size_t const cSize = ZSTD_compress(
        compressed.data(), compressed.size(),
        data.data(), data.size(),
        3  // compression level
    );

    if (ZSTD_isError(cSize)) {
        throw CompressionError(
            std::format("Zstd compression failed: {}",
                       ZSTD_getErrorName(cSize))
        );
    }

    compressed.resize(cSize);
    return compressed;
}
```

3. **实现decompressWithZstd**:
```cpp
std::expected<std::vector<std::uint8_t>, std::string>
IDCompressorImpl::decompressWithZstd(std::span<const std::uint8_t> data) {
    if (data.empty()) {
        return std::vector<std::uint8_t>{};
    }

    // 获取原始大小
    unsigned long long const rSize = ZSTD_getFrameContentSize(
        data.data(), data.size()
    );

    if (rSize == ZSTD_CONTENTSIZE_ERROR) {
        return std::unexpected("Invalid Zstd frame");
    }
    if (rSize == ZSTD_CONTENTSIZE_UNKNOWN) {
        return std::unexpected("Zstd content size unknown");
    }

    std::vector<std::uint8_t> decompressed(rSize);

    size_t const dSize = ZSTD_decompress(
        decompressed.data(), decompressed.size(),
        data.data(), data.size()
    );

    if (ZSTD_isError(dSize)) {
        return std::unexpected(
            std::format("Zstd decompression failed: {}",
                       ZSTD_getErrorName(dSize))
        );
    }

    return decompressed;
}
```

**验收标准**:
- ✅ ID流压缩比 > 5x (Delta + Varint + Zstd)
- ✅ id_compressor_property_test 全部通过
- ✅ 往返一致性测试通过
- ✅ 不增加解压时间超过10%

**工作量估算**: 4小时
**优先级**: 🔴 P0
**依赖**: Zstd库已在Conan中配置

---

### ISSUE-P0-002: Decompress命令核心逻辑未实现

**位置**: `/workspace/src/commands/decompress_command.cpp:195-265`

**问题描述**:
Decompress命令的核心逻辑包含大量TODO占位符，导致无法正常解压FQC文件。

**代码片段**:
```cpp
// 行 195-196
// TODO: Actually open and validate the archive
// This is a placeholder for Phase 2/3 implementation

// 行 222-232
// TODO: Determine which blocks to process based on range
// Placeholder: assume single block

// 行 242-243
// TODO: Write decompressed reads to output
// Placeholder implementation
```

**影响分析**:
- 解压功能: ❌ 完全不可用
- 端到端测试: ❌ 必然失败
- MVP功能: ❌ 不完整
- 用户影响: 🔴 严重 - 无法使用工具

**根本原因**:
Phase 2集中精力实现压缩流程，解压流程留待Phase 5完成。

**修复方案**:

1. **实现archive打开和验证** (替换行195-220):
```cpp
int DecompressCommand::execute() {
    try {
        // 打开FQC archive
        FQC_LOG_INFO("Opening archive: {}", options_.inputPath.string());
        format::FQCReader reader(options_.inputPath);
        reader.open();

        // 读取GlobalHeader
        const auto& globalHeader = reader.globalHeader();
        FQC_LOG_INFO("Archive: {} reads, {} blocks",
                     globalHeader.totalReadCount,
                     reader.blockCount());

        // 验证兼容性
        if (globalHeader.version.major != format::CURRENT_VERSION_MAJOR) {
            throw FormatError(
                std::format("Incompatible version: {}.{}, expected {}.x",
                           globalHeader.version.major,
                           globalHeader.version.minor,
                           format::CURRENT_VERSION_MAJOR)
            );
        }

        // 创建输出流
        auto output = createOutputStream(options_.outputPath);

        // ... (continue with decompression)
    } catch (const FQCException& e) {
        FQC_LOG_ERROR("Decompression failed: {}", e.what());
        return static_cast<int>(e.code());
    }
}
```

2. **实现block范围解析** (替换行222-232):
```cpp
// 解析range参数
std::optional<ReadRange> range;
if (!options_.range.empty()) {
    auto parseResult = parseReadRange(
        options_.range,
        globalHeader.totalReadCount
    );
    if (!parseResult) {
        FQC_LOG_ERROR("Invalid range: {}", options_.range);
        return EXIT_FAILURE;
    }
    range = *parseResult;
    FQC_LOG_INFO("Range: {} to {}", range->start, range->end);
}

// 确定需要处理的blocks
std::vector<BlockId> blocksToProcess;
if (range) {
    blocksToProcess = reader.getBlocksForRange(range->start, range->end);
} else {
    // 处理所有blocks
    for (BlockId i = 0; i < reader.blockCount(); ++i) {
        blocksToProcess.push_back(i);
    }
}

FQC_LOG_INFO("Processing {} blocks", blocksToProcess.size());
```

3. **实现解压和输出** (替换行242-265):
```cpp
// 加载Reorder Map (如果需要原始顺序)
if (options_.originalOrder) {
    if (!(globalHeader.flags & format::flags::kHasReorderMap)) {
        throw ArgumentError("Archive does not contain reorder map");
    }
    reader.loadReorderMap();
    FQC_LOG_INFO("Loaded reorder map");
}

// 配置decompressor
algo::BlockCompressorConfig config;
config.readLengthClass = format::getReadLengthClass(globalHeader.flags);
config.qualityMode = format::getQualityMode(globalHeader.flags);

// 处理每个block
std::size_t totalReads = 0;
for (BlockId blockId : blocksToProcess) {
    // 读取block
    auto blockData = reader.readBlock(blockId);

    // 解压
    algo::BlockCompressor compressor(config);
    auto result = compressor.decompress(
        blockData.header,
        blockData.idsData,
        blockData.seqData,
        blockData.qualData,
        blockData.auxData
    );

    // 输出FASTQ格式
    for (const auto& read : result->reads) {
        *output << "@" << read.id << "\n"
                << read.sequence << "\n"
                << "+\n"
                << read.quality << "\n";
    }

    totalReads += result->reads.size();
}

FQC_LOG_INFO("Decompressed {} reads", totalReads);
return EXIT_SUCCESS;
```

4. **辅助函数实现**:
```cpp
struct ReadRange {
    ReadId start;  // 1-based inclusive
    ReadId end;    // 1-based inclusive
};

std::expected<ReadRange, std::string> parseReadRange(
    std::string_view rangeStr,
    ReadId totalReads) {

    auto colonPos = rangeStr.find(':');
    if (colonPos == std::string_view::npos) {
        return std::unexpected("Invalid range format, expected start:end");
    }

    ReadRange range;

    // 解析start
    std::string startStr(rangeStr.substr(0, colonPos));
    if (startStr.empty()) {
        range.start = 1;  // ":end" = "1:end"
    } else {
        auto [ptr, ec] = std::from_chars(
            startStr.data(),
            startStr.data() + startStr.size(),
            range.start
        );
        if (ec != std::errc{}) {
            return std::unexpected("Invalid start value");
        }
    }

    // 解析end
    std::string endStr(rangeStr.substr(colonPos + 1));
    if (endStr.empty()) {
        range.end = totalReads;  // "start:" = "start:totalReads"
    } else {
        auto [ptr, ec] = std::from_chars(
            endStr.data(),
            endStr.data() + endStr.size(),
            range.end
        );
        if (ec != std::errc{}) {
            return std::unexpected("Invalid end value");
        }
    }

    // 验证范围
    if (range.start < 1) {
        return std::unexpected("start must be >= 1");
    }
    if (range.start > range.end) {
        return std::unexpected("start must be <= end");
    }
    if (range.end > totalReads) {
        FQC_LOG_WARNING("end {} exceeds total reads {}, capping to {}",
                        range.end, totalReads, totalReads);
        range.end = totalReads;
    }

    return range;
}
```

**验收标准**:
- ✅ 可以正常解压FQC文件
- ✅ 端到端往返一致性测试通过: `compress → decompress → diff`
- ✅ range参数正常工作
- ✅ original-order参数正常工作
- ✅ 各种输出格式正常工作

**工作量估算**: 1-2天
**优先级**: 🔴 P0
**依赖**: FQCReader, BlockCompressor::decompress

---

### ISSUE-P0-003: Pipeline Sequence解压使用placeholder

**位置**: `/workspace/src/pipeline/pipeline_node.cpp:1216-1217`

**问题描述**:
Pipeline的sequence解压节点返回placeholder数据，而非真实解压的序列。

**代码片段**:
```cpp
// TODO: Implement proper sequence decompression using BlockCompressor::decompress
// For now, return placeholder sequences
return std::vector<std::string>(readCount, "PLACEHOLDER");
```

**影响分析**:
- 解压数据: ❌ 完全错误
- 往返一致性: ❌ 必然失败
- 数据损坏: 🔴 严重
- Pipeline测试: ❌ 全部失败

**根本原因**:
Pipeline实现时先完成了框架，解压逻辑计划Phase 5集成BlockCompressor。

**修复方案**:

找到placeholder代码位置（DecompressSequenceNode或类似），替换为：

```cpp
std::vector<std::string> decompressSequences(
    const format::BlockHeader& blockHeader,
    std::span<const std::uint8_t> idsData,
    std::span<const std::uint8_t> seqData,
    std::span<const std::uint8_t> qualData,
    std::span<const std::uint8_t> auxData) {

    // 配置BlockCompressor
    algo::BlockCompressorConfig config;
    config.readLengthClass = /* 从header提取 */;
    config.qualityMode = /* 从header提取 */;

    algo::BlockCompressor compressor(config);

    // 调用真实解压
    auto result = compressor.decompress(
        blockHeader, idsData, seqData, qualData, auxData
    );

    if (!result) {
        throw DecompressionError("Failed to decompress block");
    }

    // 提取sequences
    std::vector<std::string> sequences;
    sequences.reserve(result->reads.size());
    for (const auto& read : result->reads) {
        sequences.push_back(read.sequence);
    }

    return sequences;
}
```

**验收标准**:
- ✅ 返回真实解压的序列数据
- ✅ pipeline_property_test 全部通过
- ✅ 往返一致性测试通过

**工作量估算**: 4小时
**优先级**: 🔴 P0
**依赖**: BlockCompressor::decompress 已完整实现

---

## P1 级别问题（影响性能/可用性）

### ISSUE-P1-001: Info命令全为TODO占位符

**位置**: `/workspace/src/commands/info_command.cpp:95-156`

**问题描述**:
Info命令的execute方法为空实现，无法查看archive元信息。

**修复方案**:
使用FQCReader读取并格式化输出GlobalHeader、BlockIndex等信息。

**工作量估算**: 1天
**优先级**: 🟡 P1

---

### ISSUE-P1-002: GlobalAnalyzer使用长度差替代Hamming距离

**位置**: `/workspace/src/algo/global_analyzer.cpp:574-600`

**问题描述**:
重排序时使用简化的长度差计算相似度，而非真实的Hamming距离。

**影响**: 压缩比降低5-10%

**修复方案**:
实现真实的Hamming距离计算（考虑反向互补）。

**工作量估算**: 1-2天
**优先级**: 🟡 P1

---

### ISSUE-P1-003: Reorder Map保存逻辑未实现

**位置**:
- `/workspace/src/commands/compress_command.cpp:591`
- `/workspace/src/pipeline/pipeline.cpp:658`

**问题描述**:
Compress命令在finalize阶段未保存Reorder Map到archive。

**影响**: `--original-order`参数无法使用

**修复方案**:
在CompressCommand::execute的finalize阶段调用：
```cpp
if (analysisResult.reorderingPerformed && options_.saveReorderMap) {
    writer.writeReorderMap(
        analysisResult.forwardMap,
        analysisResult.reverseMap
    );
}
```

**工作量估算**: 1天
**优先级**: 🟡 P1

---

## P2 级别问题（次要优化）

### ISSUE-P2-001: CompressedStream未实现的格式

**位置**: `/workspace/src/io/compressed_stream.cpp:161-163`

**问题描述**:
部分压缩格式检测未实现。

**优先级**: 🟢 LOW

---

## 修复执行计划

### Week 1: P0问题修复
- Day 1: ISSUE-P0-001 (IDCompressor Zstd)
- Day 2-3: ISSUE-P0-002 (Decompress命令)
- Day 4: ISSUE-P0-003 (Pipeline解压)
- Day 5: 集成测试验证

### Week 2: P1问题修复
- Day 1: ISSUE-P1-001 (Info命令)
- Day 2-3: ISSUE-P1-002 (Hamming距离)
- Day 4: ISSUE-P1-003 (Reorder Map)
- Day 5: 性能benchmark

### Week 3: 测试与优化
- Day 1-2: 完整集成测试套件
- Day 3-4: 性能benchmark和优化
- Day 5: 文档更新

---

**文档版本历史**:
- v1.0 (2026-01-27): 初始版本，35个问题
- v2.0 (2026-01-29): 精简为关键问题，增加详细修复方案
