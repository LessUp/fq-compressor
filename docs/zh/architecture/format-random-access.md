---
title: FQC 格式与随机访问
description: 归档布局如何支持直接 block 定位
---

# FQC 格式与随机访问

`.fqc` 容器不是简单的字节流压缩外壳，而是一个按 block 组织的归档格式。
其布局定义在 `include/fqc/format/fqc_format.h` 中，读写实现分别位于 `include/fqc/format/fqc_writer.h` 与 `include/fqc/format/fqc_reader.h`。

## 文件布局

高层顺序如下：

```text
magic + version
-> global header
-> block payloads
-> optional reorder map
-> block index
-> file footer
```

这个顺序是有意设计的。
payload 区域在压缩期间保持 append-only，等 writer 知道全部 block 的偏移和大小后，才写入 block index 与 footer。

## 每个 block 保存什么

一个 block 表示一段有界 read 区间，并把逻辑流分开存储：

- identifiers
- sequences
- qualities
- auxiliary data，例如可变 read length

`include/fqc/format/fqc_reader.h` 用 `BlockData` 暴露读取结果，`include/fqc/format/fqc_writer.h` 用 `BlockPayload` 表示写入侧数据。
这种拆分让各条流可以各自选择合适的 codec，也允许 reader 按需跳过不需要的内容。

## 全局元数据与标志位

`include/fqc/format/fqc_format.h` 还定义了归档级 flags：

- paired-end 或 single-end 布局
- quality 与 ID 处理模式
- preserve-order 还是 reordered archive layout
- 是否带 reorder map
- 是否处于 streaming mode

reader 依靠这些标志判断能否恢复原始顺序，以及应走哪条解码路径。

## 这里的“随机访问”是什么意思

在本项目里，随机访问的粒度是 block。
reader 打开归档后先读取 footer 和 index，然后用 index entry 直接 seek 到目标 read 区间所在的 block，而不需要重放所有前序 block。

这也是 `FQCReader::open()` 先加载元数据、`FQCReader::readBlock()` 可以直接按 block id 读取的原因。
一旦 index 进入内存，访问成本相对归档总长度近似常数：读元数据、一次 seek、再只解码目标 block 的 payload。

## reorder map 的作用

重排能提高压缩局部性，但如果不持久化映射，就会丢失原始 read 顺序。
`include/fqc/format/reorder_map.h` 同时保存两种方向：

- original id -> archive id
- archive id -> original id

该映射经过 delta + varint 编码，以便可以跟随 archive 一起保存而不过度膨胀。
当解压需要恢复原始顺序时，reverse map 告诉 writer 应如何输出；当定向查询从原始 read 坐标出发时，forward map 则告诉 reader 应该访问哪些 archive 位置。

## 为什么坚持 block-based 格式

项目想同时满足三件事：面向 FASTQ 的专用压缩、可控内存、直接检索。
如果把全部内容塞进单一的整体熵编码流，某些压缩率也许会更高，但直接 seek 的代价会显著上升。
block 容器正是这里的折中：每个 block 足够自包含，能被独立解码；footer 与 index 又让整个 archive 在全局上可导航。

## 代码锚点

- 格式定义：`include/fqc/format/fqc_format.h`
- 写入路径：`include/fqc/format/fqc_writer.h`、`src/format/fqc_writer.cpp`
- 读取路径：`include/fqc/format/fqc_reader.h`、`src/format/fqc_reader.cpp`
- 重排元数据：`include/fqc/format/reorder_map.h`、`src/format/reorder_map.cpp`
