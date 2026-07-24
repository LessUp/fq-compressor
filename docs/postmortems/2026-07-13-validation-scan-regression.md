# 校验扫描致 Illumina 压缩回退到 45.84 MiB/s

- 日期：2026-07-13
- 严重度：medium
- 状态：closed
- 引入点：v2 phase 3（pre-75b7400）
- 相关：`src/format/v2_archive.cpp`、`src/commands/v2_archive_engine.cpp`

## 症状

在 writer 与 reader 边界加入严格的 logical-record 校验后，Illumina 类数据的压缩速度从 ~53 MiB/s 跌到 45.84 MiB/s，跌破 50 MiB/s 的 SLA 下限。

## 复现

```
FQC_PERF_SIZES=64 FQC_PERF_REPEATS=3 FQC_PERF_DATA=random FQC_PERF_MEMORY_MIB=64 \
FQC_PERF_ENFORCE_SLA=1 tests/e2e/test_performance.sh
```

在第一版校验实现上运行，compress 下限失败。

## 调查

- 对校验路径做 profile：sequence 与 quality 数据被额外扫了第二遍，纯粹用于校验，与 stream measurement 及 exception-decode 循环已做的工作重复。
- 确认校验逻辑本身正确，问题只在放置位置。

## 根因

校验被实现成独立的一遍 pass，而非 fuse 进已有的 measurement 与 decode 循环，导致每条 record 被遍历两次。

## 修复

将 logical-record 校验 fuse 进已有的 stream-measurement 与 exception-decoding 循环，使每条 record 只遍历一次。最终三轮中位数恢复到 53.15 MiB/s compress / 182.40 MiB/s decompress，校验逻辑保留不变。

## 验证

`FQC_PERF_ENFORCE_SLA=1` 通过；`v2_archive_test` 中的 corruption 与 truncation 测试仍通过。

## 后续与教训

性能 harness 强制执行 SLA，未来若有校验改动重新引入冗余扫描，会在 CI 中失败而非静默回退。完整优化记录见 `performance/optimizations/0001-fuse-validation-scan.md`。
