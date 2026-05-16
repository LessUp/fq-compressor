---
title: 流水线
description: 压缩与解压流水线的边界划分
---

# 流水线

fq-compressor 围绕“有界 block 流水线”组织：串行输入、并行 block 处理、串行归档输出。
这一拆分直接体现在 `include/fqc/pipeline/` 的公共接口，以及 `src/commands/` 中的命令装配代码里。

## 压缩路径

压缩命令串起五类职责：

1. **输入打开与 FASTQ 解析**：`include/fqc/io/fastq_parser.h`、`include/fqc/io/compressed_stream.h`
2. **全局分析**：`include/fqc/algo/global_analyzer.h`，决定 reads 如何分组以及是否需要重排元数据
3. **chunk 整理**：`include/fqc/pipeline/pipeline.h`，把解析结果转换成按逻辑流组织的视图
4. **block 压缩**：`include/fqc/algo/block_compressor.h`、`include/fqc/algo/id_compressor.h`、`include/fqc/algo/quality_compressor.h`
5. **归档写入**：`include/fqc/format/fqc_writer.h`、`include/fqc/pipeline/writer_node.h`

实际数据流可概括为：

```mermaid
flowchart LR
    A[FASTQ 或压缩 FASTQ] --> B[Parser]
    B --> C[全局分析与重排规划]
    C --> D[Read chunks]
    D --> E[并行 block 压缩]
    E --> F[FQC writer]
    F --> G[.fqc archive]
```

关键边界是 block。
分析阶段可能观察全量输入，但 payload 编码与归档存储始终保持在 block 级别，这样既能利用多核并行，也不会失去后续直接按 block 定位的能力。

## 解压路径

解压按相同边界反向展开：

```mermaid
flowchart LR
    A[.fqc archive] --> B[FQC reader]
    B --> C[block 定位]
    C --> D[并行逻辑流解码]
    D --> E[可选的原始顺序恢复]
    E --> F[FASTQ writer]
```

Reader 侧的关键锚点是 `include/fqc/format/fqc_reader.h`、`include/fqc/pipeline/fqc_reader_node.h` 与 `include/fqc/pipeline/decompressor_node.h`。
`src/commands/decompress_command.cpp` 负责决定本次运行是完整恢复还是定向提取。

## 为什么这样拆分

### 串行入口与出口

FASTQ 解析和最终归档写出天然带顺序语义。
把这两个阶段保持为串行，可以避免为 record framing、footer 更新和最终输出顺序引入额外线程协调。

### 并行 block 处理

一旦某个 chunk 拥有稳定的 archive-order read 范围，每个 block 就可以独立压缩或解压。
这也是 `include/fqc/pipeline/pipeline.h` 用 `ReadChunk` 和 `CompressedBlock` 表示完整工作单元的原因。

### 显式背压

流水线被设计成不会无限累积在途数据。
`include/fqc/pipeline/pipeline.h` 定义了 `kDefaultMaxInFlightBlocks`，而 `include/fqc/common/memory_budget.h` 则控制流水线在任一时刻允许持有的总数据量。

## 与命令层的关系

CLI 本身保持很薄。
`src/main.cpp` 负责参数解析，`src/commands/compress_command.cpp` 与 `src/commands/decompress_command.cpp` 把线程数、内存限制、顺序保持、校验和输出模式等选项转换成流水线配置。

这样大部分运行时行为都留在可复用的库代码里，而不是散落在 CLI 分支判断中。

## 代码锚点

- 流水线类型与协调：`include/fqc/pipeline/pipeline.h`、`src/pipeline/pipeline.cpp`
- 压缩节点：`include/fqc/pipeline/reader_node.h`、`include/fqc/pipeline/writer_node.h`
- 解压节点：`include/fqc/pipeline/fqc_reader_node.h`、`include/fqc/pipeline/decompressor_node.h`、`include/fqc/pipeline/fastq_writer_node.h`
- 命令装配：`src/commands/compress_command.cpp`、`src/commands/decompress_command.cpp`

## 下一步可阅读

- [FQC 格式与随机访问](/zh/architecture/format-random-access)：继续看 block 边界为什么在外部可见
- [学院](/zh/academy/)：如果你接下来要执行命令而不是继续读实现
