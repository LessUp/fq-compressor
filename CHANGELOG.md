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

## [0.1.0] - 2026-03-07

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
- 压缩输入：透明解压 `.gz`、`.bz2`、`.xz`、`.zst` 格式的 FASTQ 文件
- 双端测序：支持交错（interleaved）与分文件双端模式
- 内存预算：自动检测系统内存，动态分块处理大型数据集
- 退出码映射：所有 CLI 命令统一退出码（0-5）

**工程化**
- C++23 现代化代码库
- Modern CMake 构建系统
- Conan 2.x 依赖管理
- GitHub Actions CI/CD 自动化
- 预编译二进制发布（Linux x64/arm64、macOS）
- Docker 容器化支持

### Fixed
- CI 质量门禁：全项目 62 个源文件 clang-format 格式化
- `.gitignore` 移除 `package-lock.json` 排除项，修复文档 CI `npm ci` 失败
- `scripts/lint.sh` 拆分源文件查找逻辑，排除测试文件避免 `rapidcheck.h` not found 错误
- `docs-pages.yml` / `docs-quality.yml` 路径触发器增加 `package-lock.json`

---

## [0.1.0-alpha.3] - 2026-02-25

### Fixed

**源码 Bug 修复**
- `main.cpp`: `--threads`、`--memory-limit`、`--no-progress` 等全局 CLI 选项未传递到命令实现
- `main.cpp`: `-v` 标志无效果（`verbosity >= 1` 设置的日志级别仍为 `kInfo`）
- `main.cpp`: `verify` 命令为空壳，未接入已实现的 `VerifyCommand` 类
- `main.cpp`: compress 命令多个 CLI 选项（`input2`、`maxBlockBases`、`scanAllLengths` 等）未传递

**DevContainer Bug 修复**
- `setup-sshd.sh`: `collect_keys()` 返回值 bug（`return $keys_found` 在找到密钥时返回 1）
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
- `tests/CMakeLists.txt`: 注册缺失的 `fastq_parser_property_test` 和 `pipeline_property_test`
- `scripts/run_tests.sh`: 重写为基于 CMake preset 系统，修复 GCC/Clang Debug 构建目录映射错误
- Shell 脚本统一 shebang (`#!/usr/bin/env bash`)，修复多处参数引用和退出码问题
- Python 基准脚本添加输入验证和错误处理

**DevContainer 优化**
- 删除冗余的 `devcontainer.simple.json`
- SSHD 脚本移至 `.devcontainer/scripts/`
- 移除 clangd 废弃参数 `--suggest-missing-includes`

**Docker 构建**
- 生产 Dockerfile 构建阶段移除不需要的开发工具
- 统一 `USE_CHINA_MIRROR` 默认值为 `0`
- healthcheck 从 `pgrep -x bash` 改为 `gcc --version`

---

## [0.1.0-alpha.2] - 2026-01-29

### Fixed

**CMake 预设工具链路径**
- `CMakePresets.json`: 拆分 `conan-gcc-base` / `conan-clang-base` 为 Debug/Release 版本，修复 Release 构建找不到 Conan 工具链文件的问题

**FASTQ 解析器 gzip 输入支持**
- `FastqParser::open()` 使用 `openInputFile()` 替代 `std::ifstream`，支持 `.gz`/`.bz2`/`.xz` 压缩输入
- `canSeek()` 对压缩文件返回 `false`

**编译器警告（全面修复）**
- 类型转换警告 (`-Wconversion`, `-Wsign-conversion`): 21 处显式转换
- Switch 语句警告: `error.cpp` 补全所有 `ErrorCode` 枚举处理
- 未使用代码警告: 删除 `isStdinTty()`、添加 `[[maybe_unused]]`
- ODR 违规: `ReorderMapData` → `LoadedReorderMapData`
- Clang 特有: `types.h` 构造函数参数遮蔽修复
- `std::format` → `fmt::format`（Clang + libstdc++ 兼容性）

### Changed
- Clang 预设移除 `-stdlib=libc++`，改用 `libstdc++`
- LTO 标志从 `-flto` 改为 `-flto=thin`

---

## [0.1.0-alpha.1] - 2026-01-23

### Added
- `.devcontainer/devcontainer.clion.json`: JetBrains CLion 专用配置，解决 `initializeCommand` 和 Docker 构建上下文兼容性问题

### Changed
- `docker-compose.yml`: 新增可配置的宿主机数据目录挂载（`FQCOMPRESSOR_HOST_DATA_DIR`）
- DevContainer Features: 添加 `common-utils` (Zsh + Oh My Zsh)
- VS Code 扩展增强: GitLens, Git Graph, Docker, Better C++ Syntax
- Clangd 优化: `--all-scopes-completion`, `--pch-storage=memory`
- Docker 配置: healthcheck, `init: true`, `security_opt` (ptrace)
- `.dockerignore`: 新增，优化构建上下文

---

## [0.1.0-alpha.0] - 2026-01-14

### Added

**设计文档**
- 参考项目文档: fastq-tools、fqzcomp5、HARC、minicom、Spring、repaq、pigz、NanoSpring 共 8 个参考项目
- README 更新: Key Features、Core Algorithms、Engineering Optimizations、References 章节

**核心设计决策**
- **两阶段压缩策略 (Two-Phase Compression)**: Phase 1 全局分析 + Phase 2 分块压缩，解决 Spring ABC 与 Block-based 随机访问的矛盾
- **ABC + SCM 路线确定**: Sequence = Spring ABC, Quality = fqzcomp5 SCM, IDs = Tokenization + Delta
- **FQC 容器格式**: Header + Metadata + Block 区 + Index/Trailer + checksums，支持随机访问
- **Reorder Map**: Delta + Varint 压缩，约 2 bytes/read
- **Codec 版本兼容策略**: Family 变更不兼容，Version 变更向后兼容

**规格完善**
- CLI 细化: 子命令、退出码、`--range`、`--header-only` 等参数定义
- 容器格式前向兼容: GlobalHeaderSize、header_size、checksum_type、codec (family,version)
- Paired-End 支持: Interleaved / Consecutive 存储布局
- Long Read 模式: 自动检测（median length > 10KB）、策略差异表
- 内存管理: 默认 8192MB 上限、超大文件自动分治
- 格式字段修复: Stream Offsets `uint32_t` → `uint64_t`, Flags `uint32` → `uint64`
- 最终一致性校对: 需求编号追踪闭环、架构图与模块命名对齐

**工程化配置**
- `scripts/lint.sh`、`scripts/build.sh`、`scripts/test.sh`、`scripts/install_deps.sh` — 统一构建/测试/检查脚本
- `.pre-commit-config.yaml` — clang-format、cmake-format、shellcheck、commitizen 等钩子
- `.clang-tidy` 增强: clang-analyzer、cppcoreguidelines、readability-magic-numbers (阈值 25)
- `.editorconfig` 完善: 全局 charset/indent、CMake/Python/Docker 专用规则
- `docs/dev/coding-standards.md`、`docs/dev/project-setup.md` — 编码规范与项目工程化文档

**TBB 并行与性能优化**
- `src/pipeline/pipeline.cpp`: 启用 `tbb::parallel_pipeline` 三阶段（Reader → Compressor → Writer）实现真正多线程并行压缩/解压
- `BufferPool`: 使用 64 字节缓存行对齐 (`std::aligned_alloc`)，提升 SIMD 和缓存性能

**架构优化**
- `pipeline_node.cpp` 拆分为 6 个独立文件：`reader_node`、`compressor_node`、`writer_node`、`fqc_reader_node`、`decompressor_node`、`fastq_writer_node`
- `main.cpp`: 消除 5 个全局可变变量，所有选项改为局部变量
- `compressor_node.cpp`: `calculateBlockChecksum()` 从简单哈希替换为 `XXH3_64bits` streaming API

### Fixed

**性能关键修复 (P0)**
- `pipeline.cpp`: Compressor round-robin 数据竞争 — 改为创建 `maxInFlightBlocks` 个实例
- `pipeline.cpp`: PE 压缩并行化 — 替换串行循环为 TBB `parallel_pipeline`
- `compressor_node.cpp`: 序列压缩零拷贝 `ReadRecordView`，消除冗余深拷贝
- `decompressor_node.cpp`: 缓存 `BlockCompressor` 复用，避免每块重建 Zstd context
- `reader_node.cpp`: 新增 `AsyncStreamBuf` 适配器，对非压缩文件自动启用 async prefetch

**Docker 修复**
- `docker/docker-compose.yml`: 构建代理变量切换为 `DEVCONTAINER_*`，避免自动继承宿主机代理

### Changed

**规格文档完善**
- 设计一致性修复: 版本语义、索引兼容、流式模式约束、分治顺序、校验范围等 7 项不一致点
- Spec 深度评审: 15 项修复（采样边界条件、`--range` 参数语义、`--streams` 输出格式、ReorderMap 前向兼容等）
- Spring 读长兼容性: 补充 511bp 限制约束，自动检测加入 `max_length` 判定
- 长读边界条件: 引入 `max_read_length >= 100KB` 的 Ultra-long 判定，明确 Zstd vs BSC 取舍
- 编码规范统一: 对齐 clang-format/clang-tidy/editorconfig，补充 Git 提交规范

**DevContainer/Docker 同步**
- DevContainer 脚本、镜像构建与 SSH 访问策略全面同步
- 新增生产镜像 `Dockerfile`
- 更新 `docker-compose.yml`、`.env`、`.env.example` 统一 SSH 端口与代理配置
