# 架构

架构部分说明 fq-compressor 如何由解析、分析、压缩、存储和访问模块组合而成。
重点是边界与数据流，让读者无需通读所有源码也能理解系统结构。

```mermaid
flowchart LR
    A[FASTQ parser] --> B[全局分析器]
    B --> C[block 压缩器]
    C --> D[FQC writer 与索引]
    D --> E[Reader、verify、范围解码]
```

## 本节目的

- 描述主要子系统及其拆分原因
- 说明压缩、索引与随机访问职责分别位于何处
- 将公开概念与仓库中的实现位置对应起来

## 本节应回答的问题

- FASTQ 输入如何经过流水线写入 FQC 归档
- 哪些组件负责格式、算法与命令编排
- 若要继续深入，应在代码库哪里查找

## 下一步可阅读

- [流水线](/zh/architecture/pipeline)：查看压缩与解压的数据流边界
- [FQC 格式与随机访问](/zh/architecture/format-random-access)：查看归档布局与 block 定位
- [I/O 与内存](/zh/architecture/io-memory)：查看缓冲与内存预算行为
- [学院](/zh/academy/)：查看面向操作者的流程
- [研究](/zh/research/)：查看源码、规格与支撑材料
