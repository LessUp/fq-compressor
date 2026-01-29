# 2026-01-29 修复 FASTQ 解析器的 gzip 压缩输入支持

## 问题描述

使用 `.fq.gz` 格式的 gzip 压缩 FASTQ 文件作为输入时，压缩命令报错：

```
[format error] Invalid FASTQ format at line 1: expected '@' at start of ID line
```

## 根本原因

`FastqParser::open()` 方法使用普通的 `std::ifstream` 直接打开文件，而没有使用 `openInputFile()` 函数来透明地处理压缩格式。这导致：

1. gzip 压缩的二进制数据被当作纯文本 FASTQ 格式解析
2. 第一行的 gzip 魔术字节不是 `@`，触发格式错误

此外，`canSeek()` 方法没有考虑压缩流的情况。对于压缩流，`seekg` 操作不被支持，但 `canSeek()` 仍返回 `true`，导致 `sampleRecords()` 之后的 `reset()` 操作使流状态失效。

## 修复内容

### include/fqc/io/fastq_parser.h

- 添加 `isCompressed_` 成员变量跟踪文件是否压缩
- 修改 `canSeek()` 方法：对压缩文件返回 `false`

### src/io/fastq_parser.cpp

- 在 `open()` 方法中使用 `openInputFile()` 替代 `std::ifstream`
- 使用 `detectCompressionFormatFromExtension()` 检测文件压缩格式
- 添加 `compressed_stream.h` 头文件

## 验证结果

### gzip 压缩输入 (test_small.fq.gz)
```
Total reads:      100
Compression ratio: 3.14x
```

### 未压缩输入 (test_small.fq)
```
Total reads:      100
Compression ratio: 3.14x
```

## 影响范围

- 修复了对 `.gz`、`.bz2`、`.xz` 等压缩格式的输入文件支持
- 保持对未压缩 `.fq`/`.fastq` 文件的兼容性
- 压缩文件不再支持 `reset()`/seek 操作（这是预期行为）
