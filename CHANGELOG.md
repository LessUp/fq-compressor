# 变更记录

## [未发布]

### 修复

- **并发流水线死锁**：`SpscQueue` 新增 `abort()` 机制，writer 写入失败时中止队列，解除 reader 在满队列 `push` 上的自旋阻塞。此前 writer 失败后 reader 会永久卡在 `push`，导致 `reader.join()` 挂死。
- **双向唤醒对称性**：`push` 返回 `bool` 并在自旋时检查中止标志，`pop` 在空队列时检查中止与关闭，与 `close()` 形成完整的双向唤醒。
- **SPSC `pop()` 丢失尾帧竞态**：`head_` 与 `closed_` 是独立 atomic，`pop` 先读 `head_` 再读 `closed_` 时，消费者可能看到 stale `head_` 却看到 `closed_=true`，误判队列空而返回 `nullopt`，丢失 `close` 前最后一次 `pushFrame` 的帧。修复为观察到 `closed_/aborted_` 后再 acquire 重读 `head_` 做终检。→ 详见 [docs/postmortems/2026-07-24-spsc-pop-lost-tail-frame.md](docs/postmortems/2026-07-24-spsc-pop-lost-tail-frame.md)

### 变更

- **接入并发主路径**：`ArchiveEngine::compress` 改用 `CompressPipeline`（reader/writer 双线程 + 有界 SPSC 队列）替代串行 `FrameAccumulator`，压缩流水线真正并发生效。此前 `CompressPipeline` 已存在但无任何调用点。
- `CompressPipeline::run` 扩展支持样本注入（`initialRecords`）、双端输入（`mate`）、逻辑字节统计（`logicalBytes`）。
- 删除未用的 `FrameAccumulator` 与 `retainedRecordBytes`。

### 删除

- `benchmark_v2/`（13 个 Python 文件）、`scripts/benchmark_v2.sh`、`tests/e2e/benchmark_v2_smoke_test.sh`（共约 2700 行）：多工具选型框架与单编码器场景无关，性能测量由 `tests/e2e/test_performance.sh` 覆盖。

### 测试

- 新增 `WriterFailureAbortsReaderWithoutDeadlock`（死锁验证）、`PairedRun`、`InitialRecordsEmittedFirst` 及 SPSC 队列中止场景单测。
- clang-debug / clang-asan（ASan+UBSan）/ clang-tsan（TSan）三套构建下全部 8/8 通过，无内存错误与数据竞争。

### 构建

- 修复 `scripts/build.sh` 调用已删除的 `install_deps.sh` 的隐患，缺失 Conan 工具链时改为明确报错与指引。
- 修正 `tests/README.md` 对已删除 `acceptance.sh` 与 `benchmark_v2_smoke_test.sh` 的失效引用。
