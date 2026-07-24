# SPSC `pop()` 在 `close` 后丢失尾帧

- 日期：2026-07-24
- 影响版本：未发布（开发中）
- 引入点：`SpscQueue` 多 atomic 设计自始存在，`CompressPipeline::run` 接入并发主路径后暴露
- 严重度：数据正确性——压缩结果静默少帧，无报错

## 症状

CI `pipeline_test` 中 `CompressPipelineTest.PairedRun` 偶发失败：

```
tests/pipeline/pipeline_test.cpp:107: Failure
Expected equality of these values:
  result->recordCount
    Which is: 0
  10U
    Which is: 10
```

特征：
- 仅 CI（GitHub Actions `ubuntu-24.04` runner）出现，本地 x86 反复跑 200 次不复现
- 失败的是**最后一个**测试用例 `PairedRun`，前序全过
- `recordCount=0` 而非部分值——一帧都没消费到
- 无 sanitizer 报错，无崩溃

## 复现

**本地无法稳定复现。** 关键观察：

- `clang-debug` 200 次、`clang-tsan` 50 次 PairedRun 全过
- CI 同一 commit 反复跑，失败率约 10–30%
- 失败窗口极窄：5 条 paired record（10 条总）体积小，全部累积进**同一帧**，靠 `close` 前最后一次 `pushFrame` 落入队列

`PairedRun` 的特殊性在于：输入小到不触发任何 `frameFull()` 切分，整批记录的生命周期是

```
reader: append×10 → pushFrame(1 帧) → close()
writer: pop() → ... → pop() → nullopt
```

若 writer 的 `pop` 在 reader 的 `pushFrame` 与 `close` 之间进入空判分支，且内存序使其看到 `closed_=true` 却没看到 `head_` 已推进，就会直接返回 `nullopt`，丢掉那唯一一帧。

## 根因

`SpscQueue` 用三个独立 atomic 协调状态：

```cpp
alignas(64) std::atomic<std::size_t> head_{0};   // 生产者写
alignas(64) std::atomic<std::size_t> tail_{0};   // 消费者写
alignas(64) std::atomic<bool> closed_{false};    // 生产者写
```

原 `pop()`：

```cpp
while (true) {
    const auto tail = tail_.load(relaxed);
    if (tail != head_.load(acquire)) { /* 取出 */ return item; }
    if (aborted_.load(acquire) || closed_.load(acquire)) {
        return std::nullopt;   // ← bug
    }
    yield();
}
```

问题在 `head_` 与 `closed_` 是**两个独立 atomic**，无共同释放点。考虑生产者序列：

```
push:   head_.store(h+1, release)
close:  closed_.store(true, release)
```

消费者同一轮迭代：

```
1. head_.load(acquire)  →  可能读到旧值 h（=tail，判空）
2. closed_.load(acquire) →  读到 true
```

`closed_` 的 acquire 只与 `closed_.store(release)` 建 happens-before，**不保证**步骤 1 的 `head_` load 能看到 `push` 的 store。在弱内存模型（CI runner 的虚拟化环境比裸 x86 TSO 更易暴露）下，步骤 1 可读到 stale `head_`，步骤 2 却读到新 `closed_`，于是误判"队列空且已关闭"，返回 `nullopt`。

x86 TSO 下 store→store 与 load→load 不重排，故本地难复现；CI 的并发时序更易命中窗口。这是典型的"多 atomic 状态机缺统一同步点"陷阱。

## 修复

`include/fqc/pipeline/spsc_queue.h`，`pop()` 在观察到 `closed_/aborted_` 后**再 acquire 重读一次 `head_`**：

```cpp
if (aborted_.load(acquire) || closed_.load(acquire)) {
    if (tail != head_.load(acquire)) {   // 终检：close 的 release 已使 push 的 head store 可见
        T item = std::move(storage_[tail]);
        tail_.store((tail + 1) & kMask, release);
        return item;
    }
    return std::nullopt;
}
```

为何这次重读有效：`closed_.load(acquire)` 读到 `true` 时，已与生产者 `closed_.store(release)` 建立 happens-before；而生产者的 `head_.store(release)` 被序列在 `closed_.store` 之前（同一线程，program order + release-release），故其写对消费者此刻可见。第二次 `head_.load(acquire)` 必能看到 `push` 推进的 head。

注：这是最小修复，不引入新同步原语。更彻底的方案是改用单一 atomic 打包 head/closed（如 seqlock 或高位标志位），但会改动整个队列的 push/pop 路径，收益与风险不成比例。

## 验证

- `clang-tsan` 构建，PairedRun 单跑 300 次：全过，无 ThreadSanitizer 告警
- `clang-tsan` 全套 8 个测试：100% 通过
- CI（`ubuntu-24.04`，clang-18）修复后 run `30093223771`：全绿
- 修复 commit：`71d62f3`

## 教训

1. **多 atomic 状态机需显式同步点。** `head_`、`closed_`、`aborted_` 各自 acquire/release 只保证单变量可见性，不保证跨变量的 load 顺序。若消费者逻辑依赖"先读 A 再读 B"的隐含顺序，必须有一处同时建立两者的 happens-before，否则在弱内存模型下会观察到"新 B + 旧 A"的撕裂快照。

2. **`close` 不是 flush。** `close()` 只声明"不再有新 push"，不等价于"之前的 push 都已可见"。后者需要 `close` 与 `push` 之间有明确的 release-acquire 链。本例中链是存在的（同线程 program order + release store），但消费者必须在**读到 close 之后**重读 head 才能搭上这条链。

3. **CI flaky ≠ 环境噪声。** 本地不复现、CI 偶发的测试失败，尤其是"数据少了一个/多了"而非"崩溃/超时"，往往指向真实的内存序或并发缺陷。不要用 `--retry` 掩盖。

4. **小输入是竞态的放大器。** `PairedRun` 用 10 条小记录恰好使整批落入一帧，把竞态窗口压缩到 `pushFrame→close` 这一对操作上，反而比大输入更易命中。压力测试用大输入测吞吐，用**最小输入**测竞态边界。

5. **TSan 不万能。** TSan 检测数据竞争，但本 bug 是合法 acquire/release 下的"可见性顺序"问题，无 race（每次 load/store 都正确配对），TSan 不报。最终靠根因推理 + 修复后回归确认。
