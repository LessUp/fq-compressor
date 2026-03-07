# fq-compressor Spec 完整性评审报告

**日期**: 2026-01-15
**评审范围**: requirements.md, design.md, tasks.md, ref.md 完整性与一致性检查

## 评审状态: ✅ 通过

所有 spec 文档已完成，设计决策一致，可以开始实施。

---

## 1. 核心设计决策总结

### 1.1 压缩策略分层

| Read 分类 | 长度条件 | Sequence 压缩 | Quality 压缩 | Reordering | Block Size |
|-----------|----------|---------------|--------------|------------|------------|
| **Short** | median < 1KB 且 max ≤ 511 | Spring ABC | SCM Order-2 | ✅ 启用 | 100K reads |
| **Medium** | 1KB ≤ median < 10KB 或 max > 511 | Zstd | SCM Order-2 | ❌ 禁用 | 50K reads |
| **Long** | median ≥ 10KB | **Zstd (推荐)** | SCM Order-1 | ❌ 禁用 | 10K reads |

### 1.2 Spring 511bp 限制处理

**关键约束**: Spring ABC 的 `MAX_READ_LEN = 511` 是硬编码常量，深度嵌入 bitset 数据结构。

**设计决策**:
- ✅ 不扩展 Spring ABC 用于长读（收益有限，工作量大）
- ✅ max_length > 511 的数据自动切换到 Medium 策略
- ✅ 使用 Zstd 作为 Medium/Long 的主要压缩策略

### 1.3 NanoSpring 评估结论

**结论**: NanoSpring 不是 Spring 的扩展，而是完全不同的算法。

**本项目策略**:
- ✅ Long Read 使用 **Zstd 作为推荐首选**（简单、稳定）
- ⚠️ Overlap-based 作为 Phase 5+ 可选优化（需适配 Block 模式，复杂度高）

---

## 2. 文档一致性检查

### 2.1 requirements.md ↔ design.md

| 检查项 | 状态 | 说明 |
|--------|------|------|
| Read 长度分类定义 | ✅ | 两文档一致：Short/Medium/Long 三级 |
| Spring 兼容性约束 | ✅ | 两文档均明确 max > 511 → Medium |
| 压缩策略选型 | ✅ | ABC/SCM/Zstd 策略一致 |
| 流式模式定义 | ✅ | stdin 自动启用流式模式 |

### 2.2 design.md ↔ tasks.md

| 检查项 | 状态 | 说明 |
|--------|------|------|
| 长读检测阈值 | ✅ | Task 18.1 与 design.md 一致 |
| 压缩策略实现 | ✅ | Task 18.2 明确 Zstd fallback |
| Block Size 配置 | ✅ | Task 12.2 与 design.md 一致 |

### 2.3 ref.md 完整性

| 检查项 | 状态 | 说明 |
|--------|------|------|
| Spring 限制说明 | ✅ | 已添加 MAX_READ_LEN = 511 说明 |
| NanoSpring 对比 | ✅ | 已添加算法差异对比表 |
| 使用策略建议 | ✅ | 已明确各场景推荐策略 |

---

## 3. 实施风险评估

### 3.1 高风险项

| 风险项 | 风险等级 | 缓解措施 |
|--------|----------|----------|
| Spring 代码集成 | **高** | Phase 2 预留 4-6 周；优先验证核心算法 |
| 两阶段压缩策略 | **高** | 先实现单阶段验证正确性 |
| 内存管理 | **中** | 实现 MemoryBudget 监控；分治模式 |

### 3.2 技术债务

| 项目 | 优先级 | 说明 |
|------|--------|------|
| Overlap-based 长读压缩 | 低 | Zstd 已足够，可作为 Phase 5+ 优化 |
| QVZ 有损压缩 | 低 | Task 9.3 标记为可选 |

---

## 4. 下一步行动

### 4.1 可以开始实施

spec 文档已完成，可以开始 Phase 1 实施：

1. **Task 1**: 项目初始化与构建系统搭建
2. **Task 2**: 基础设施模块实现
3. **Task 3**: FQC 文件格式实现

### 4.2 实施建议

1. **优先验证 Spring 集成可行性**: 在 Phase 1 完成后，建议先做 Spring 代码分析（Task 7.1），评估实际集成难度
2. **保持 Zstd fallback 路径**: 确保 Medium/Long 模式可独立工作，不依赖 Spring ABC
3. **属性测试优先**: 每个模块完成后立即编写属性测试，确保往返一致性

---

## 5. 文档版本

| 文档 | 最后更新 | 状态 |
|------|----------|------|
| requirements.md | 2026-01-15 | ✅ 已审核 |
| design.md | 2026-01-15 | ✅ 已审核 |
| tasks.md | 2026-01-15 | ✅ 已审核 |
| ref.md | 2026-01-15 | ✅ 已审核 |

---

## 附录: 关键代码引用

### Spring MAX_READ_LEN 定义

```cpp
// ref-projects/Spring/src/params.h
const uint16_t MAX_READ_LEN = 511;
const uint32_t MAX_READ_LEN_LONG = 4294967290;  // -l 模式使用
const int NUM_READS_PER_BLOCK = 256000;
const int NUM_READS_PER_BLOCK_LONG = 10000;
```

### 长度分类检测逻辑

```cpp
// 自动检测逻辑 (优先级从高到低):
// 1. max_length >= 10KB → LONG
// 2. max_length > 511 → MEDIUM (Spring 兼容保护)
// 3. median >= 1KB → MEDIUM
// 4. 其余 → SHORT
```
