# fq-compressor 需求概览

## 目标

fq-compressor 的核心目标是构建一个面向 FASTQ 数据的高性能压缩工具，在以下维度取得平衡：

- 高压缩比
- 高吞吐并行处理
- 随机访问能力
- 工程可维护性
- 面向短读与长读场景的可扩展性

## 核心压缩路线

- **Sequence**
  - 采用基于 Spring / Mincom 思路的 **Assembly-based Compression (ABC)**。

- **Quality**
  - 采用借鉴 fqzcomp5 的 **Statistical Context Mixing (SCM)**。

- **Identifiers**
  - 采用 **Tokenization + Delta**。

## 容器与随机访问

- 输出格式为 `.fqc`
- 文件以 **Block** 为单位组织
- Footer 与 Block Index 支持快速定位
- 支持 `--range` 等按范围解压能力

## CLI 需求

### 子命令

- `compress`
- `decompress`
- `info`
- `verify`

### 关键选项

- `-t, --threads`
- `-i, --input`
- `-o, --output`
- `-l, --level`
- `--quality-mode`
- `--id-mode`
- `--range`
- `--split-pe`
- `--original-order`
- `--quick`
- `--fail-fast`

## 数据模式需求

- 支持 **SE / PE**
- 支持 **保序 / 重排**
- 支持 **无损质量值 / 有损质量值 / 丢弃质量值**
- 支持 **短读 / 中长读 / 超长读** 的分类压缩策略

## 性能目标

| 指标 | 目标区间 |
|------|----------|
| 压缩率 | 0.4 - 0.6 bits/base |
| 压缩速度 | 20 - 50 MB/s |
| 解压速度 | 100 - 200 MB/s |
| 内存峰值 | < 4 GB |

## 工程与构建需求

- C++20
- CMake 3.20+
- Conan 2.x
- Intel oneTBB
- CLI11 / Quill / xxHash
- Google Test / RapidCheck

## 质量与可维护性需求

- 使用 `.clang-format` 与 `.clang-tidy`
- CI 需要覆盖格式检查、静态分析、构建与测试
- 使用 `CMakePresets.json` 统一本地与 CI 构建行为

## 数据完整性需求

- 文件级校验
- Block 级校验
- `verify` 命令独立校验归档完整性
- 支持损坏块隔离与跳过策略

## 版本兼容性需求

- 新版本 Reader 应尽量兼容旧版本归档
- 文件头与索引结构要预留扩展字段
- 对未知 codec family 明确报错

## 边界条件需求

- 支持空 FASTQ 文件
- 长读超过 Spring 原生短读限制时自动切换通用压缩策略
- stdin 模式下自动退化为更保守的流式策略

## 当前文档状态

本文件是对原始需求文档的收敛版摘要。若你需要保留更细粒度的逐条验收标准，建议后续继续把原始 requirements 细项增量迁入本目录。
