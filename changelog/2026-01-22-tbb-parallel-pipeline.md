# 2026-01-22 TBB Parallel Pipeline 实现

## 变更概述

启用 TBB `parallel_pipeline` 实现真正的多线程并行压缩/解压。

## 修改的文件

### `src/pipeline/pipeline.cpp`

1. **启用 TBB headers** (第18-20行)
   - 取消注释 `#include <tbb/parallel_pipeline.h>` 和 `#include <tbb/task_arena.h>`

2. **重构压缩管线** (第244-381行)
   - 替换串行 while 循环为 `tbb::parallel_pipeline` 三阶段实现
   - Stage 1 (Reader): `serial_in_order` - 串行读取 FASTQ chunks
   - Stage 2 (Compressor): `parallel` - 并行压缩 chunks
   - Stage 3 (Writer): `serial_in_order` - 串行写入保持顺序

3. **重构解压管线** (第664-804行)
   - 同样使用 `tbb::parallel_pipeline` 三阶段实现
   - Stage 1 (FQC Reader): 串行读取压缩块
   - Stage 2 (Decompressor): 并行解压缩
   - Stage 3 (FASTQ Writer): 串行写入

## 预期性能提升

| 场景 | 优化前 | 优化后 (预期) |
|------|--------|---------------|
| 单线程 | 1x | 1x |
| 8 核并行 | 1x (串行) | 6-8x |
| 16 核并行 | 1x (串行) | 10-14x |

## 验证

- [x] CMake 配置成功
- [x] 编译通过 (GCC 15.1.0, Release, LTO 启用)
- [x] 所有测试通过 (2/2)
