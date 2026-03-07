# fq-compressor 项目问题清单与解决方案

## 问题统计
- **总问题数**: 35个占位符/TODO标记
- **P0级 (阻塞)**: 6个 - 无法压缩/解压
- **P1级 (严重)**: 8个 - 功能缺失
- **P2级 (中等)**: 15个 - 品质问题
- **P3级 (低)**: 6个 - 优化机会

---

## P0 - 阻塞性问题 (必须立即解决)

### ISSUE-P0-001: compress命令仅写魔数,未实现实际压缩

**位置**: `/workspace/src/commands/compress_command.cpp:251-305`

**现象**:
- `runCompression()` 函数读取FASTQ但不压缩
- 输出文件只有8字节魔数 `89 46 51 43 0D 0A 1A 0A`
- 行298: `outFile.write("\x89" "FQC\r\n" "\x1a\n", 8);`

**根本原因**:
- 缺少块切分逻辑
- 缺少GlobalAnalyzer调用
- 缺少BlockCompressor调用
- 缺少FQCWriter集成

**关键代码片段**:
```cpp
void CompressCommand::runCompression() {
    // 行251-305: 完全是占位符
    // 仅计数读取,未实际压缩
    while (auto record = parser.readRecord()) {
        ++readCount;
        baseCount += record->length();
    }
    // 直接写8字节魔数并返回
    std::ofstream outFile(options_.outputPath, std::ios::binary);
    outFile.write("\x89" "FQC\r\n" "\x1a\n", 8);
    stats_.outputBytes = 8;  // Placeholder
}
```

**所需修复**:
```
1. 块切分循环
   for (size_t i = 0; i < reads.size(); i += blockSize) {
     auto block = reads.subspan(i, min(blockSize, reads.size()-i));
     compressAndWriteBlock(block, blockId++);
   }

2. 重排序分析 (Phase 1)
   GlobalAnalyzer analyzer;
   auto reorderMap = analyzer.analyze(allReads);

3. 块压缩 (Phase 2)
   BlockCompressor compressor;
   auto compressed = compressor.compress(block, config);

4. 写入FQC格式
   FQCWriter writer(outputPath);
   writer.writeHeader();
   writer.writeBlock(compressed);
   writer.writeIndex();
   writer.writeFooter();
```

**工作量**: 2-3天
**风险**: 低 - BlockCompressor已80%完成
**优先级**: 🔴 P0 - 阻塞所有测试

---

### ISSUE-P0-002: decompress命令未实现,仅写占位符

**位置**: `/workspace/src/commands/decompress_command.cpp:228-265`

**现象**:
- `runDecompression()` 只写注释行
- 行254: `*output << "# Decompression not yet implemented\n";`
- 无法还原压缩文件

**根本原因**:
- 缺少FQCReader调用
- 缺少BlockCompressor::decompress()调用
- 缺少FASTQ输出逻辑
- 缺少Reorder Map处理

**所需修复**:
```
1. 读取FQC文件头
   FQCReader reader(inputPath);
   auto header = reader.readHeader();

2. 遍历每个块
   for (BlockId id = 0; id < header.totalBlocks; ++id) {
     auto blockData = reader.readBlock(id);

3. 解压块
     BlockCompressor decompressor;
     auto decompressed = decompressor.decompress(blockData);

4. 写FASTQ输出
     for (const auto& read : decompressed->reads) {
       output << "@" << read.id << "\n"
              << read.sequence << "\n"
              << "+\n"
              << read.quality << "\n";
     }
   }

5. 恢复原始顺序 (如果有Reorder Map)
   if (options_.originalOrder) {
     auto reorderMap = reader.readReorderMap();
     // 按reorderMap重新排序输出
   }
```

**工作量**: 2天
**风险**: 中 - 需要与BlockCompressor::decompress()接口匹配
**优先级**: 🔴 P0 - 阻塞所有测试

---

### ISSUE-P0-003: FQCWriter未完全实现,缺少块写入方法

**位置**: `/workspace/src/format/fqc_writer.cpp`

**现象**:
- `writeBlock()` 方法未实现
- `writeIndex()` 方法未实现
- `writeFooter()` 方法未实现

**所需修复**:
1. `writeBlock(const CompressedBlock& block)` - 写入块头+数据
2. `writeIndex()` - 写入块索引数组
3. `writeFooter()` - 写入文件footer
4. 块校验和计算集成

**工作量**: 1-2天
**风险**: 低 - 格式已清晰定义
**优先级**: 🔴 P0

---

### ISSUE-P0-004: FQCReader未完全实现,缺少块读取方法

**位置**: `/workspace/src/format/fqc_reader.cpp:334-340`

**现象**:
- 行334-335: `// TODO: Decompress maps (Delta + Varint decoding)`
- `readBlock()` 方法不完整
- `readIndex()` 不完整
- Reorder Map解压缺失

**所需修复**:
1. `readBlock(BlockId)` - 读取并返回压缩块数据
2. `readIndex()` - 解析块索引数组
3. Reorder Map解压 - Delta + Varint 解码
4. 校验和验证

**工作量**: 1-2天
**优先级**: 🔴 P0

---

### ISSUE-P0-005: GlobalAnalyzer重排序逻辑未实现

**位置**: `/workspace/src/algo/global_analyzer.cpp`

**现象**:
- Minimizer索引框架存在但算法未完成
- 全局重排序决策未实现
- Reorder Map生成逻辑缺失

**所需修复**:
1. Minimizer bucketing算法
2. Approximate Hamiltonian Path (重排序决策)
3. Reorder Map编码 (Delta + Varint)

**工作量**: 3-5天
**风险**: 高 - 复杂的图论算法
**优先级**: 🔴 P0 (但可暂用简化版本跳过重排序)

**临时方案**: Short Read 可先跳过重排序,直接块压缩

---

### ISSUE-P0-006: main.cpp命令调用为占位符

**位置**: `/workspace/src/main.cpp:363-393`

**现象**:
- 行370: `FQC_LOG_INFO("Compress command not yet implemented");`
- 行380: `FQC_LOG_INFO("Decompress command not yet implemented");`
- 命令直接返回,未调用实现

**所需修复**:
```cpp
// 当前 (占位符)
case Commands::kCompress: {
    FQC_LOG_INFO("Compress command not yet implemented");
    return 0;
}

// 应改为
case Commands::kCompress: {
    auto cmd = createCompressCommand(...);
    return cmd->execute();
}
```

**工作量**: 0.5天 (一旦compress_command实现完成)
**优先级**: 🔴 P0

---

## P1 - 严重问题 (影响核心功能)

### ISSUE-P1-001: pipeline_node.cpp多个占位符

**位置**: `/workspace/src/pipeline/pipeline_node.cpp:618-1244`

**问题列表**:
1. 行618-619: `// TODO: Actually compress with Zstd`
2. 行629: `// Simple checksum calculation (placeholder)`
3. 行796: `// TODO: Parse and write reorder map`
4. 行1216-1217: `// TODO: Implement proper sequence decompression`
5. 行1241: `// Fallback: return placeholder qualities`

**影响**: TBB流水线节点实现
**工作量**: 2-3天 (Phase 4)
**优先级**: 🟡 P1 (非MVP关键)

---

### ISSUE-P1-002: IDCompressor中Zstd集成缺失

**位置**: `/workspace/src/algo/id_compressor.cpp:963,972`

**问题**:
- 行963: `// TODO: Integrate actual Zstd compression`
- 行972: `// TODO: Integrate actual Zstd decompression`

**影响**: ID流压缩
**工作量**: 1天 (调用Zstd库)
**优先级**: 🟡 P1 (可选)

---

### ISSUE-P1-003: fqc_reader.cpp Reorder Map解压

**位置**: `/workspace/src/format/fqc_reader.cpp:334-340`

**问题**:
```cpp
// TODO: Decompress maps (Delta + Varint decoding)
// For now, store placeholder - actual decompression will be implemented
std::vector<uint32_t> forward(readCount);
std::iota(forward.begin(), forward.end(), 0);
// Placeholder: identity mapping
return {forward, forward};
```

**影响**: 无法恢复原始read顺序
**工作量**: 1天
**优先级**: 🟡 P1

---

### ISSUE-P1-004: Info命令未实现

**位置**: `/workspace/src/commands/info_command.cpp:95-155`

**问题**:
- 行95-96: `// TODO: Read and display GlobalHeader`
- 行155: `// TODO: Iterate through blocks`

**影响**: 用户无法查看归档信息
**工作量**: 1-2天
**优先级**: 🟡 P1 (辅助功能)

---

### ISSUE-P1-005-008: 其他7个P1问题

1. pipeline.cpp:658 - Reorder Map加载
2. main.cpp:116 - corrupted placeholder处理
3. decompress_command.h:90 - corruptedPlaceholder默认值
4. pipeline_node.cpp多个质量/ID占位符

**处理**: 主要在Phase 5阶段解决
**优先级**: 🟡 P1

---

## P2 - 中等问题 (品质问题)

### ISSUE-P2-001到P2-015: 质量值和序列占位符

**位置**:
- quality_compressor.cpp:811 - 缺失质量值占位符
- block_compressor.cpp:1328,1336 - 丢弃质量模式占位符
- pipeline_node.cpp:554,1104,1124,1128,1199,1241,1244 - 多个占位符

**影响**: 质量值丢弃模式处理
**处理**: Phase 2-3中逐步解决
**优先级**: 🟢 P2

---

## P3 - 低优先级问题 (优化机会)

### ISSUE-P3-001: compressed_stream.cpp未完全实现

**位置**: `/workspace/src/io/compressed_stream.cpp:161`

**问题**: bzip2压缩流返回false (未实现)

**优先级**: 🟢 P3 (可选扩展)

---

## 解决方案优先级时间表

### Week 1 - MVP打通
- ✅ P0-001: compress命令 (2-3天)
- ✅ P0-002: decompress命令 (2天)
- ✅ P0-003: FQCWriter完善 (1-2天)
- ✅ P0-004: FQCReader完善 (1-2天)
- ⏸️ P0-005: GlobalAnalyzer (跳过重排序,直接块压缩)
- ✅ P0-006: main.cpp集成 (0.5天)

### Week 2 - 测试与稳定性
- ✅ P1-003: Reorder Map解压
- ✅ P1-001: pipeline_node修复
- 📝 所有单元测试通过
- 📝 属性测试通过
- 📝 端到端验证

### Week 3 - 并行优化
- ✅ TBB流水线实现 (P1-001关联)
- ✅ 性能验证

### Week 4 - 完善与发布
- ✅ P1-004: Info命令
- ✅ P1-002: Zstd集成
- ✅ P2系列: 占位符消除
- ✅ Benchmark报告

---

## 验证检查清单

### 压缩功能验证
- [ ] compress命令输出有效FQC文件(不是8字节)
- [ ] 输出文件包含GlobalHeader
- [ ] 输出文件包含BlockIndex
- [ ] 输出文件包含Footer

### 解压功能验证
- [ ] decompress命令读取FQC文件
- [ ] 输出有效FASTQ文件
- [ ] 往返一致性: compress → decompress == 原文件

### 测试通过
- [ ] 所有单元测试PASSED
- [ ] 所有属性测试PASSED
- [ ] 集成测试PASSED (157MB真实数据)

### 性能目标
- [ ] 压缩率 > 2.5x (Short Read)
- [ ] 单线程速度 > 50 MB/s
- [ ] 4线程加速比 > 3x

### Benchmark
- [ ] 生成benchmark_report.md
- [ ] 生成GCC vs Clang对比图表

---

## 相关文件快速索引

### 核心实现文件
| 问题ID | 文件 | 行数 | 紧急度 |
|--------|------|------|--------|
| P0-001 | compress_command.cpp | 251-305 | 🔴 |
| P0-002 | decompress_command.cpp | 228-265 | 🔴 |
| P0-003 | fqc_writer.cpp | - | 🔴 |
| P0-004 | fqc_reader.cpp | 334-340 | 🔴 |
| P0-005 | global_analyzer.cpp | - | 🔴 |
| P1-001 | pipeline_node.cpp | 618-1244 | 🟡 |

### 已完成的核心模块 (可直接使用)
| 模块 | 行数 | 状态 |
|------|------|------|
| BlockCompressor | 1626 | ✅ 80%+ |
| QualityCompressor | 911 | ✅ 90%+ |
| IDCompressor | 1054 | ✅ 90%+ |
| FastqParser | 完整 | ✅ 100% |
| FQC格式定义 | 完整 | ✅ 100% |
