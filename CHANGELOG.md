# Changelog

本文件记录 fq-compressor 项目的所有重要变更。

格式基于 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.0.0/)，遵循 [Conventional Commits](https://www.conventionalcommits.org/zh-hans/)。

版本号遵循 [语义化版本](https://semver.org/lang/zh-CN/) 规范：`MAJOR.MINOR.PATCH`

---

## [Unreleased]

### Added

- 文档体系完善：CHANGELOG、README、RELEASE 规范化更新
- 性能基准测试框架：支持 GCC/Clang 编译器对比、多维度性能分析

### Changed

- 文档结构优化：统一命名规范、增强技术细节说明

---

## [0.1.0] - 2026-03-22

### Added

**核心功能**

- ABC 压缩算法：基于共识序列的增量编码，适用于短读段（< 300bp）
- SCM 质量压缩：统计上下文模型 + 算术编码，高效压缩质量分数
- 多压缩格式支持：gzip、bzip2、xz、zstd、libdeflate 透明读写
- 随机访问：块索引归档格式，支持高效部分解压
- 并行处理：基于 Intel TBB 的并行块压缩/解压
- 管线模式：3 阶段 Reader→Compressor→Writer 管线，支持反压（`--pipeline`）
- 异步 I/O：后台预取与写入缓冲，提升吞吐
- 流式模式：从标准输入低内存压缩，无需全局重排
- 压缩输入：透明解压 `.gz`、`.bz2`、`.xz` 格式的 FASTQ 文件（`.zst` FASTQ 输入暂未支持）
- 双端测序：支持交错（interleaved）与分文件双端模式
- 内存预算：自动检测系统内存，动态分块处理大型数据集
- 退出码映射：所有 CLI 命令统一退出码（0-5）

**工程化**

- C++23 现代化代码库（GCC 14+ / Clang 18+）
- Modern CMake 构建系统（3.28+）
- Conan 2.x 依赖管理
- GitHub Actions CI/CD 自动化
- 预编译二进制发布（Linux x64/arm64、macOS）
- Docker 容器化支持（GCC 15 + Clang 21 双编译器）

### Changed

**2026-03-22 — Original-order decompression MVP**

- 在 `decompress` 命令中实现基于 reorder map 的 `--original-order` 单线程恢复路径
- `--range` / `--range-pairs` 与 `--original-order` 的组合语义：先按 archive 顺序选择子集，再按 original order 输出
- 为 `split-pe` 的 original-order 路径增加 paired archive 校验

**2026-03-22 — Documentation and licensing alignment**

- 修复 README 文档中的 Benchmark 链接
- 明确项目许可证口径：项目自研代码采用 MIT，`vendor/spring-core/` 保留其原始 Spring 非商业研究用途许可证
- 新增根级 `LICENSE` 文件

**2026-03-22 — Explicit unsupported-feature handling**

- `decompress` 命令在参数校验阶段对 `--original-order` 显式报错
- 文档补充输入格式支持说明：支持 plain/gzip/bzip2/xz，`.zst` FASTQ 输入暂未支持

**2026-03-10 — GitHub Pages optimization**

- `pages.yml` paths 触发范围收窄为 `docs/gitbook/**`
- 添加 sparse-checkout，仅拉取必要文件
- Honkit 文档站链接修复与中文搜索支持

**2026-03-08 — Toolchain stability fixes**

- Clang 20 → Clang 21（LLVM 21 qualification branch）
- CMake 最低版本提升至 3.28
- 移除 Dockerfile 中与 Conan 冲突的 dev 包

### Fixed

- CI 质量门禁：全项目 62 个源文件 clang-format 格式化
- `.gitignore` 移除 `package-lock.json` 排除项，修复文档 CI `npm ci` 失败
- `scripts/lint.sh` 拆分源文件查找逻辑，排除测试文件避免 `rapidcheck.h` not found 错误

---

## [0.1.0-alpha.3] - 2026-02-25

### Fixed

**源码 Bug 修复**

- `main.cpp`: `--threads`、`--memory-limit`、`--no-progress` 等全局 CLI 选项未传递到命令实现
- `main.cpp`: `-v` 标志无效果（`verbosity >= 1` 设置的日志级别仍为 `kInfo`）
- `main.cpp`: `verify` 命令为空壳，未接入已实现的 `VerifyCommand` 类
- `main.cpp`: compress 命令多个 CLI 选项（`input2`、`maxBlockBases`、`scanAllLengths` 等）未传递

**DevContainer Bug 修复**

- `setup-sshd.sh`: `collect_keys()` 返回值 bug
- `container-setup.sh`: SSH agent 检查逻辑修复

### Changed

**代码质量**

- 消除重复枚举 `QualityCompressionMode`，统一使用 `QualityMode`
- 消除匿名命名空间中 `CompressOptions` 等类型的名称遮蔽

**文档重构**

- `docs/` 目录下 32 个文件统一为 kebab-case 命名
- `docs/reivew/` → `docs/review/`（修正拼写错误）
- 约 15 个文件的交叉引用同步更新

**Tests & Scripts**

- `tests/CMakeLists.txt`: 注册缺失的测试目标
- `scripts/run_tests.sh`: 重写为基于 CMake preset 系统
- Shell 脚本统一 shebang (`#!/usr/bin/env bash`)

---

## [0.1.0-alpha.2] - 2026-01-29

### Fixed

**CMake 预设工具链路径**

- `CMakePresets.json`: 拆分 base preset 为 Debug/Release 版本

**FASTQ 解析器 gzip 输入支持**

- `FastqParser::open()` 支持压缩输入文件

**编译器警告**

- 类型转换警告: 21 处显式转换
- Switch 语句警告: 补全所有枚举处理
- Clang 特有警告修复

### Changed

- Clang 预设移除 `-stdlib=libc++`，改用 `libstdc++`
- LTO 标志从 `-flto` 改为 `-flto=thin`

---

## [0.1.0-alpha.1] - 2026-01-23

### Added

- `.devcontainer/devcontainer.clion.json`: JetBrains CLion 专用配置

### Changed

- `docker-compose.yml`: 新增可配置的宿主机数据目录挂载
- DevContainer Features: 添加 `common-utils`
- VS Code 扩展增强: GitLens, Git Graph, Docker

---

## [0.1.0-alpha.0] - 2026-01-14

### Added

**设计文档**

- 8 个参考项目文档: fastq-tools、fqzcomp5、HARC、minicom、Spring、repaq、pigz、NanoSpring
- README 更新: Key Features、Core Algorithms、Engineering Optimizations

**核心设计决策**

- 两阶段压缩策略 (Two-Phase Compression)
- ABC + SCM 路线确定
- FQC 容器格式定义
- Reorder Map 压缩方案
- Codec 版本兼容策略

**工程化配置**

- 统一构建/测试/检查脚本
- `.pre-commit-config.yaml` 钩子
- `.clang-tidy` 增强
- `.editorconfig` 完善

### Fixed

**性能关键修复**

- `pipeline.cpp`: Compressor round-robin 数据竞争修复
- `pipeline.cpp`: PE 压缩并行化
- `compressor_node.cpp`: 序列压缩零拷贝优化
- `decompressor_node.cpp`: 缓存 BlockCompressor 复用

---

[Unreleased]: https://github.com/LessUp/fq-compressor/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/LessUp/fq-compressor/compare/v0.1.0-alpha.3...v0.1.0
[0.1.0-alpha.3]: https://github.com/LessUp/fq-compressor/compare/v0.1.0-alpha.2...v0.1.0-alpha.3
[0.1.0-alpha.2]: https://github.com/LessUp/fq-compressor/compare/v0.1.0-alpha.1...v0.1.0-alpha.2
[0.1.0-alpha.1]: https://github.com/LessUp/fq-compressor/compare/v0.1.0-alpha.0...v0.1.0-alpha.1
[0.1.0-alpha.0]: https://github.com/LessUp/fq-compressor/releases/tag/v0.1.0-alpha.0
