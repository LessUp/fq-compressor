---
title: I/O 与内存
description: 输入处理、缓冲策略与内存预算边界
---

# I/O 与内存

在 fq-compressor 中，I/O 与内存控制都是一等架构问题。
相关实现主要位于 `include/fqc/io/`、`include/fqc/common/memory_budget.h` 以及 `include/fqc/pipeline/` 的流水线协调代码中。

## 输入侧

输入路径强调流式处理，而不是整文件预载入。
`include/fqc/io/compressed_stream.h` 负责识别 plain FASTQ、gzip、bzip2 与 xz，并通过统一输入流暴露给上层。
随后 `include/fqc/io/fastq_parser.h` 以有界 chunk 的方式读取标准四行 FASTQ 记录。

这种设计有两个直接效果：

- 压缩输入支持不会渗入压缩算法本身
- parser 可以保持 record-aware，而底层传输层继续保持 byte-oriented

## 输出侧

归档输出同样采用分阶段缓冲。
`include/fqc/format/fqc_writer.h` 先顺序追加 blocks，并记录它们的偏移；等所有 payload 大小确定后，再统一写入 index 与 footer。
因为偏移只有在 finalize 时才完全可靠，writer 必须独占 archive 布局与 checksum 记账，而不是把这些职责分散给 worker 线程。

## Async I/O 支持

`include/fqc/io/async_io.h` 定义了基于 buffer pool 的异步 reader 与 writer。
其模型很明确：可复用缓冲区、受限的预取深度、以及 write-behind，而不是无限增长的队列。
即使当前命令路径保持较保守，这套抽象也让 I/O 与计算重叠成为可能，而无需改写 parser 或 codec 接口。

## 内存预算模型

`include/fqc/common/memory_budget.h` 直接写明了内存模型：

- phase 1 全局分析按约 24 bytes/read 估算
- phase 2 block 处理按约 50 bytes/read/active block 估算
- 默认预算会分摊到分析保留区、block buffer 与 worker stack 预留

`MemoryEstimator` 与 `ChunkPlanner` 的职责，就是判断一次运行能否单次完成，还是必须进入 divide-and-conquer chunking。
这对大数据集尤其重要：工具宁可显式切块，也不希望在途缓冲持续膨胀到不可控。

## 背压与有界并发

流水线通过两层机制维持内存纪律：

1. `include/fqc/pipeline/pipeline.h` 限制 in-flight blocks 数量
2. `include/fqc/io/async_io.h` 限制 buffer pool 深度

两者叠加后，可以同时限制未消费输入、未压缩 chunk 状态和已压缩输出的驻留规模。
这也是项目既强调并行吞吐，又能维持可预测内存上限的原因。

## 对使用者的含义

`--memory-limit` 不是一个表面参数。
它会进入预算模型，进一步影响 chunk size、预留分配，以及本次运行是否需要回退到 chunked execution。
对操作者来说，内存策略属于正常压缩行为的一部分，而不是事后补救。

## 代码锚点

- 透明输入流：`include/fqc/io/compressed_stream.h`、`src/io/compressed_stream.cpp`
- FASTQ 解析：`include/fqc/io/fastq_parser.h`、`src/io/fastq_parser.cpp`
- 异步缓冲池：`include/fqc/io/async_io.h`、`src/io/async_io.cpp`
- 预算与切块：`include/fqc/common/memory_budget.h`、`src/common/memory_budget.cpp`
- 流水线背压：`include/fqc/pipeline/pipeline.h`、`src/pipeline/pipeline.cpp`
