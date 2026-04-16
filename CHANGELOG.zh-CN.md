# 变更日志

本项目所有重要变更都将记录在此文件中。

格式基于 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)，
本项目遵循 [语义化版本](https://semver.org/lang/zh-CN/) 规范。

---

## [未发布]

### 新增

- 文档体系全面重构，支持双语（中英）
- 遵循 Keep a Changelog 标准的专业化变更日志
- GitHub 发布说明自动化

---

## [0.1.0] - 2026-04-16

### 概述

fq-compressor 首个稳定版本，完整的 FASTQ 压缩流水线、并行处理和随机访问能力。

### 新增

#### 核心压缩功能

- **ABC（基于组装的压缩）** 用于基因序列
  - 基于 Minimizer 的 reads 分桶（k=21, w=11）
  - 桶内基于 TSP 的 reads 重排序
  - 局部共识序列生成和增量编码
  - 在基因数据上实现 3.5-5 倍压缩比

- **SCM（统计上下文混合）** 用于质量分数
  - 高阶上下文建模（最高 2 阶）
  - 自适应算术编码
  - 可选的质量分箱策略（Illumina 8 分箱）
  - 平衡压缩比和信息保留

- **ID 流压缩**
  - reads 标识符的标记化
  - 单调数字令牌的增量编码
  - 常见模式的字典压缩

- **FQC 归档格式**
  - 基于块的存储架构（默认 ~10MB 块）
  - 列式流（独立的 ID/Seq/Qual 压缩）
  - 原生随机访问，O(1) 块查找
  - 多级校验和完整性验证
  - 重排映射用于原始顺序恢复

#### 并行处理

- 基于 Intel oneTBB 的生产者-消费者流水线
- 可配置线程池，自动检测核心数
- 基于令牌的流控制用于内存管理
- 通过工作窃取调度器实现负载均衡
- 线程安全的块级并行压缩

#### 命令行接口

- `compress` - FASTQ 到 FQC 压缩，支持多种模式
- `decompress` - FQC 到 FASTQ，支持完整和部分提取
- `info` - 归档元数据检查（文本和 JSON 输出）
- `verify` - 完整性验证
- `split-pe` - 双端测序归档分离

#### I/O 能力

- 透明输入解压（.gz, .bz2, .xz）
- 可配置输出压缩（zstd, zlib）
- 带预取缓冲的异步 I/O
- 双端测序支持（交错和分离文件）
- 大型数据集的内存限制操作

#### 工程与工具

- **构建系统**: 现代 CMake 3.28+ 与 Conan 2.x
- **语言**: C++23（GCC 14+, Clang 18+）
- **测试**: Google Test，34+ 测试用例
- **基准测试**: 自动化性能框架与可视化
- **容器化**: Docker 和 DevContainer 支持
- **CI/CD**: GitHub Actions 构建、测试和发布

#### 文档

- 全面的 README 与快速入门指南
- 技术规格文档
- 算法深入解析与参考资料
- 双语文档（英文/中文）
- GitBook 文档站点

### 性能

在 2.27M Illumina reads（511 MB 未压缩）上测试：

| 编译器 | 压缩速度 | 解压速度 | 压缩比 |
|--------|----------|----------|--------|
| GCC 15.2 | 11.30 MB/s | 60.10 MB/s | 3.97x |
| Clang 21 | 11.90 MB/s | 62.30 MB/s | 3.97x |

- 单线程性能
- 最高 8 核多线程扩展
- 100MB+ 文件峰值内存使用：<2GB

### 变更

- 优化序列重排序算法以获得更好的缓存局部性
- 改进子系统间内存预算分配
- 优化 CLI 参数解析以保持一致性

### 修复

- 流水线令牌管理中的竞态条件
- 质量压缩器上下文初始化的内存泄漏
- 空 FASTQ 文件的边缘情况处理

---

## [0.1.0-alpha.3] - 2026-02-25

### 修复

- `--threads`、`--memory-limit`、`--no-progress` 等 CLI 选项传递
- 详细程度标志 `-v` 日志级别调整
- `verify` 命令与 `VerifyCommand` 类集成
- DevContainer SSH 代理检测逻辑

### 变更

- 消除重复的 `QualityCompressionMode` 枚举，统一为 `QualityMode`
- 解决匿名命名空间类型遮蔽问题
- 文档文件命名统一为短横线命名法
- Shell 脚本统一使用 `#!/usr/bin/env bash` shebang

---

## [0.1.0-alpha.2] - 2026-01-29

### 修复

- CMake 预设工具链路径
- `FastqParser::open()` gzip 输入支持
- 21 处类型转换警告
- Switch 语句穷尽性警告
- Clang 特定诊断警告

### 变更

- 从 Clang 预设中移除 `-stdlib=libc++`，使用 `libstdc++`
- LTO 标志从 `-flto` 改为 `-flto=thin`

---

## [0.1.0-alpha.1] - 2026-01-23

### 新增

- CLion 专用 DevContainer 配置
- 带可配置主机数据挂载的 Docker Compose
- 额外 VS Code 扩展（GitLens、Git Graph、Docker）

### 变更

- DevContainer 功能包含 `common-utils`

---

## [0.1.0-alpha.0] - 2026-01-14

### 新增

- 初始项目结构和架构设计
- 核心压缩算法原型
- 参考项目分析（8 个项目）
- 预提交钩子配置
- EditorConfig 和格式化规则

---

## 发布说明格式

每次发布包含：

1. **二进制制品**
   - Linux x86_64（静态 musl，动态 glibc）
   - Linux ARM64（静态 musl）
   - macOS x86_64（Intel）
   - macOS ARM64（Apple Silicon）

2. **文档**
   - 安装指南
   - 快速入门教程
   - CLI 参考
   - 性能基准测试

3. **验证**
   - 所有制品的 SHA-256 校验和
   - GPG 签名（从 v1.0.0 开始）

---

[未发布]: https://github.com/LessUp/fq-compressor/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/LessUp/fq-compressor/compare/v0.1.0-alpha.3...v0.1.0
[0.1.0-alpha.3]: https://github.com/LessUp/fq-compressor/compare/v0.1.0-alpha.2...v0.1.0-alpha.3
[0.1.0-alpha.2]: https://github.com/LessUp/fq-compressor/compare/v0.1.0-alpha.1...v0.1.0-alpha.2
[0.1.0-alpha.1]: https://github.com/LessUp/fq-compressor/compare/v0.1.0-alpha.0...v0.1.0-alpha.1
[0.1.0-alpha.0]: https://github.com/LessUp/fq-compressor/releases/tag/v0.1.0-alpha.0
