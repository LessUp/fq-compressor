# fq-compressor 设计概览

## 设计目标

fq-compressor 的设计目标可概括为五点：

- **高压缩比**
- **高性能并行处理**
- **模块化与可维护性**
- **现代 C++ 工程体系**
- **基于 Block 的随机访问**

## 总体分层

### CLI 层

负责：

- 参数解析
- 子命令分发
- 日志等级与输出控制
- 用户输入/输出约束校验

### Core 层

负责：

- FASTQ 解析
- Sequence / Quality / ID 三条逻辑流压缩与解压
- `.fqc` 格式读写
- 校验与错误处理

### Pipeline 层

负责：

- 组织 `read -> compress -> write` 流水线
- 以 oneTBB 为核心实现并行调度
- 平衡吞吐、内存占用与块粒度

### Storage 层

负责：

- Block 写入与读取
- Block Index
- Reorder Map
- Footer 与全局校验

## 核心算法设计

### 1. Sequence 压缩

短读场景采用 **ABC 路线**：

- Minimizer Bucketing
- Reordering
- Local Consensus
- Delta Encoding
- Arithmetic Coding

这条路线的关键收益在于：不是直接压缩 reads 本身，而是压缩 reads 相对局部共识的差异。

### 2. Quality 压缩

质量值不适合直接沿用组装式压缩，因此采用 **SCM**：

- 构造高阶上下文
- 预测下一个质量值分布
- 使用算术编码逼近熵极限

### 3. ID 压缩

对 FASTQ header 做结构化拆分：

- Tokenization
- 整数位置 Delta
- 再交由通用压缩器处理

## 两阶段压缩策略

这是本项目的关键架构决策。

### Phase 1: 全局分析

- 提取 minimizer
- 构建 bucket
- 计算全局重排顺序
- 生成 `original_id -> archive_id` 映射

### Phase 2: 分块压缩

- 按 Block 并行压缩
- 每个 Block 独立编码
- 保证 Block 之间状态隔离
- 保留随机访问能力

该策略的核心价值是：

- 既保留全局重排带来的压缩收益
- 又保留 block-based 文件格式对随机访问的支持

## `.fqc` 文件格式要点

### 文件布局

- Magic Header
- Global Header
- Blocks
- Reorder Map（可选）
- Block Index
- File Footer

### 设计原则

- 所有 Block 独立可解码
- Footer 可从文件尾部快速读取
- Index 与 Header 预留扩展字段
- 通过 `header_size` / `entry_size` 支持前向兼容

## 长读策略

### 短读

- 适合 ABC
- 允许重排序
- 默认较大 block

### 中读 / 长读 / 超长读

- 逐步退化为更稳妥的通用压缩策略
- 默认优先 Zstd
- 禁用全局重排序
- 通过更保守的 block 粒度控制内存

原因很明确：

- Spring 风格 ABC 更适合短读
- 长读错误率与相似性分布不适合强行套用同一路线

## 数据完整性

### 校验层级

- 全局校验
- Block 校验
- `verify` 命令独立校验
- 损坏块隔离与跳过

## 工程设计原则

- C++20
- Modern CMake
- Conan 2.x
- 统一脚本入口
- CI 中执行格式检查、静态分析、构建与测试

## 当前状态

本文件是将原始详细设计文档收敛到 `docs/` 后的概览版。若你后续希望保留完整字段级别格式定义，可以继续把原始详细设计逐段迁入本目录。
