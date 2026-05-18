# 系统设计

这一节把概念叙事对应回代码库和归档布局。当你需要理解 fq-compressor 如何在解析、分析、block 压缩、格式落盘与定向检索之间分层时，就从这里开始。

<ArchitectureAtlas locale="zh" />

## 分层地图

| 层 | 职责 | 关键锚点 |
| --- | --- | --- |
| 输入 | 打开 FASTQ 与压缩 FASTQ 输入，统一流处理 | `include/fqc/io/fastq_parser.h`、`include/fqc/io/compressed_stream.h` |
| 分析 | 采集全局统计，决定重排意图与内存纪律 | `include/fqc/algo/global_analyzer.h`、`include/fqc/common/memory_budget.h` |
| 压缩 | 以 block 为单位编码序列、ID 与质量值 | `include/fqc/algo/block_compressor.h`、`include/fqc/algo/id_compressor.h`、`include/fqc/algo/quality_compressor.h` |
| 格式 | 写出 blocks、校验和、重排元数据和检索结构 | `include/fqc/format/fqc_writer.h`、`include/fqc/format/fqc_header.h` |
| 检索 | 完成校验、范围解码，必要时恢复原始顺序 | `include/fqc/format/fqc_reader.h`、`include/fqc/pipeline/decompressor_node.h` |

## 不可妥协的结构约束

- block 是同时兼容吞吐、校验作用域与随机访问的最小单元。
- 归档格式是产品契约的一部分，不是 CLI 背后的 opaque byte bucket。
- 命令层必须保持瘦身，让压缩和解压行为停留在可复用的库代码里，而不是 CLI 私有分支里。

## 下一步

- [流水线](./pipeline)：查看并发与流控
- [FQC 格式与随机访问](./format-random-access)：查看归档布局和定位语义
- [I/O 与内存](./io-memory)：查看缓冲、背压和内存预算行为
