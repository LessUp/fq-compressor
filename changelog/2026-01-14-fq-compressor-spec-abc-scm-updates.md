# fq-compressor 规格/设计升级：ABC（Spring/Mincom）+ SCM（fqzcomp5 风格）（2026-01-14）

## 背景

- 调研 2024 年相关工作（FastqZip、Spring、Mincom、fqzcomp5）后确认：仅靠“Reorder + Encoder”已不是当前最前沿的 FASTQ 压缩路线。
- 学术界与顶级工具（Spring/Mincom）的核心方向是 **Assembly-based Compression (ABC)**：不直接存储原始 reads，而是构建局部共识并存储相对共识的 edits（compress the edits）。
- 同时，质量值（Quality）更适合采用 **fqzcomp5 风格的 Statistical Context Mixing (SCM)** 上下文算术编码，作为性价比极高的质量流方案。

## 变更

- `.kiro/specs/fq-compressor/requirements.md`
  - 在引言与可行性评估中明确最终路线：Sequence=ABC（Spring/Mincom），Quality=SCM（fqzcomp5 风格），IDs=Tokenization+Delta。
  - 强化 Requirement 1.1（Strategy B）：明确 **Vendor/Fork Spring Core（encoder/decoder）** 的集成方式，而非从零重写。
  - 补充约束与风险：ABC 的 CPU/内存成本与 round-trip 无损正确性验证要求；并强调 SCM 仅用于 Quality，Sequence 不走 fqzcomp5 纯统计主路线。

- `.kiro/specs/fq-compressor/design.md`
  - 更新“设计目标/参考项目借鉴”：将 Sequence 主路线明确为 Spring/Mincom 的 ABC（minimizer bucketing / local consensus / delta / arithmetic）。
  - 细化 Sequence Stream pipeline：Stage 4 明确为 **Arithmetic Coding**，并补充 “compress the edits” 的设计动机。
  - 强调 Minimizer Bucketing 的分治作用：用于控制桶内规模，降低全量处理时的内存峰值风险。

- `.kiro/specs/fq-compressor/tasks.md`
  - Phase 2 的 Spring 集成任务表述与模块识别范围对齐 ABC：包含 minimizer bucketing、(可选) reordering、consensus/delta、arithmetic coding。
  - 增加 Spring License 约束审阅任务。
  - 将 Quality 压缩任务表述更新为 SCM（Order-1/Order-2）。

## 影响

- 文档层面：Sequence/Quality/IDs 三条子流的策略边界更清晰，且明确了工程可行的落地路径（Vendor/Fork Spring Core + 自研封装 Block 接口）。
- 实现层面：后续编码应优先聚焦 Spring 核心模块的可移植化与 Block-aware 封装，并配套与原始 Spring 的对照验证来降低正确性风险。

## 回退方案

- 回滚本次变更涉及的三份 spec 文件至变更前版本即可：
  - `.kiro/specs/fq-compressor/requirements.md`
  - `.kiro/specs/fq-compressor/design.md`
  - `.kiro/specs/fq-compressor/tasks.md`
