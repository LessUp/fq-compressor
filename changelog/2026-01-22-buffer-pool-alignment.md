# 2026-01-22 BufferPool 缓存行对齐优化

## 变更概述

优化 BufferPool 内存分配，使用 64 字节缓存行对齐，提升 SIMD 和缓存性能。

## 修改的文件

### `include/fqc/io/async_io.h`

- 在 `BufferPool` 类中添加 `AlignedDeleter` 结构体
- 定义 `AlignedBuffer` 类型别名
- 更新 `buffers_` 成员类型为 `std::vector<AlignedBuffer>`

### `src/io/async_io.cpp`

- 添加 `kCacheLineSize = 64` 常量
- 修改 `BufferPool::BufferPool()` 使用 `std::aligned_alloc`
- 缓冲区大小向上取整到缓存行边界

## 预期性能提升

| 场景 | 预期收益 |
|------|----------|
| SIMD 操作 | 无对齐错误，避免性能惩罚 |
| 多线程访问 | 减少伪共享（false sharing） |
| 缓存效率 | 更好的缓存行利用率 |

## 验证

- [x] 编译通过
- [x] 所有测试通过 (2/2)
