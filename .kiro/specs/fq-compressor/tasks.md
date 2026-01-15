# Implementation Plan: fq-compressor

## Overview

本实现计划将 fq-compressor 项目分为 5 个主要阶段，按照设计文档中的 Development Phases 组织。每个阶段包含可独立验证的任务，确保增量开发和持续集成。

实现语言：**C++20**
构建系统：**CMake 3.20+**
依赖管理：**Conan 2.x**

### 时间估算与风险评估

| Phase | 预估时间 | 风险等级 | 关键依赖 |
|-------|----------|----------|----------|
| Phase 1: Skeleton & Format | 2-3 周 | 低 | CLI11, Quill, xxHash |
| Phase 2: Spring 集成 | 4-6 周 | **高** | Spring 源码理解、两阶段策略实现 |
| Phase 3: TBB 流水线 | 2-3 周 | 中 | TBB API 熟练度 |
| Phase 4: 优化与扩展 | 2-3 周 | 中 | PE/Long Read 策略验证 |
| Phase 5: 验证与发布 | 2 周 | 低 | 测试覆盖率 |

**总计**: 12-17 周（约 3-4 个月）

---

## Phase 1: Skeleton & Format (项目骨架与格式定义)

- [ ] 1. 项目初始化与构建系统搭建
  - [ ] 1.1 创建项目目录结构
    - 创建 `fq-compressor/` 根目录
    - 创建 `include/fqc/`, `src/`, `tests/`, `vendor/` 目录
    - 创建 `.clang-format`, `.clang-tidy` 配置文件
    - _Requirements: 7.1, 7.3_

  - [ ] 1.2 配置 CMake 构建系统
    - 创建根 `CMakeLists.txt`，设置 C++20 标准
    - 配置编译选项 (warnings, sanitizers)
    - 创建 `cmake/` 目录存放 Find 模块
    - _Requirements: 7.1_

  - [ ] 1.3 配置 Conan 依赖管理
    - 创建 `conanfile.py` 或 `conanfile.txt`
    - 添加依赖：CLI11, Quill, xxHash, TBB, GTest, RapidCheck
    - 配置 CMake 集成
    - _Requirements: 7.2_

- [ ] 2. 基础设施模块实现
  - [ ] 2.1 实现 Logger 模块 (Quill)
    - 创建 `include/fqc/common/logger.h`
    - 创建 `src/common/logger.cpp`
    - 实现 `fqc::log::init()` 和全局 logger 访问
    - _Requirements: 4.2_

  - [ ] 2.2 实现通用类型定义
    - 创建 `include/fqc/common/types.h`
    - 定义 `ReadRecord`, `QualityMode`, `IDMode` 等核心类型
    - 使用 C++20 Concepts 约束模板
    - _Requirements: 7.3_

  - [ ] 2.3 实现错误处理框架
    - 创建 `include/fqc/common/error.h`
    - 定义 `FQCException` 层次结构
    - 实现 `Result<T, E>` 类型 (或使用 std::expected)
    - _Requirements: 7.3_

- [ ] 3. FQC 文件格式实现
  - [ ] 3.1 定义格式常量与数据结构
    - 创建 `include/fqc/format/fqc_format.h`
    - 定义 Magic Header (`0x89 'F' 'Q' 'C' ...`)
    - 定义 `GlobalHeader`, `BlockHeader`, `IndexEntry`, `FileFooter` 结构体
    - _Requirements: 2.1, 5.1, 5.2_

  - [ ] 3.2 实现 FQCWriter 类
    - 创建 `include/fqc/format/fqc_writer.h` 和 `src/format/fqc_writer.cpp`
    - 实现 `write_global_header()`, `write_block()`, `finalize()`
    - 实现 Block Index 构建和写入
    - 集成 xxHash64 校验和计算
    - **实现原子写入**: 使用 `.fqc.tmp` + rename 策略
    - **实现信号处理**: 捕获 SIGINT/SIGTERM，清理临时文件
    - _Requirements: 2.1, 5.1, 5.2, 8.1, 8.2_

  - [ ] 3.3 实现 FQCReader 类
    - 创建 `include/fqc/format/fqc_reader.h` 和 `src/format/fqc_reader.cpp`
    - 实现 `read_global_header()`, `read_block()`, `read_index()`
    - 实现随机访问：`seek_to_block()`, `get_reads_range()`, Reorder Map lookup (for original ID)
    - 实现 Header-only 读取：仅解码 Identifier Stream
    - 预留子流选择性解码接口：可只解码 Sequence 或 Quality 子流
    - 实现校验和验证
    - _Requirements: 2.1, 2.2, 2.3, 5.1, 5.2_

  - [ ] 3.4 编写格式读写属性测试
    - **Property 1: FQC 格式往返一致性**
    - *For any* 有效的 GlobalHeader 和 BlockData 序列，写入后读取应产生等价数据
    - **Validates: Requirements 2.1, 5.1, 5.2**

- [ ] 4. FASTQ 解析器实现
  - [ ] 4.1 实现 FASTQ 解析器核心
    - 创建 `include/fqc/io/fastq_parser.h` 和 `src/io/fastq_parser.cpp`
    - 实现流式解析，支持分块读取
    - 处理 4 行格式 (ID, Seq, +, Qual)
    - _Requirements: 1.1.1_

  - [ ] 4.2 实现压缩输入支持 (Phase 1: gzip)
    - 创建 `include/fqc/io/compressed_stream.h`
    - 集成 libdeflate 或 zlib 进行 gzip 解压
    - 实现透明的压缩格式检测
    - _Requirements: 1.1.1_

  - [ ] 4.3 编写 FASTQ 解析属性测试
    - **Property 2: FASTQ 解析往返一致性**
    - *For any* 有效的 FASTQ 记录序列，解析后重新格式化应产生等价输出
    - **Validates: Requirements 1.1.1**

- [ ] 5. CLI 框架实现
  - [ ] 5.1 实现 CLI 主入口
    - 创建 `src/main.cpp`
    - 使用 CLI11 配置子命令：`compress`, `decompress`, `info`, `verify`
    - 实现全局选项：`-t/--threads`, `-v/--verbose`, `--memory-limit`
    - **实现 stdout TTY 检测**: 非 TTY 时自动禁用进度显示
    - **实现 stdin 检测**: stdin 输入时自动启用流式模式并发出警告
    - _Requirements: 6.1, 6.2, 6.3_

  - [ ] 5.2 实现 CompressCommand 框架
    - 创建 `src/commands/compress_command.h/cpp`
    - 解析压缩相关选项：`-i`, `-o`, `-l`, `--reorder`, `--lossy-quality`
    - 添加 `--streaming` 选项支持流式模式
    - 添加 `--long-read-mode <auto|short|medium|long>` 选项
    - 预留压缩引擎调用接口
    - _Requirements: 6.2_

  - [ ] 5.3 实现 DecompressCommand 框架
    - 创建 `src/commands/decompress_command.h/cpp`
    - 解析解压选项：`-i`, `-o`, `--range`, `--header-only`
    - 添加 `--skip-corrupted` 和 `--corrupted-placeholder` 选项
    - 预留解压引擎调用接口
    - _Requirements: 6.2, 8.5_

  - [ ] 5.4 实现 InfoCommand 和 VerifyCommand
    - 创建 `src/commands/info_command.h/cpp`
    - 创建 `src/commands/verify_command.h/cpp`
    - Info: 显示归档元数据
    - Verify: 校验文件完整性
    - _Requirements: 5.3, 6.2_

- [ ] 6. Checkpoint - Phase 1 验证
  - 确保所有测试通过
  - 验证 CLI 框架可正常解析参数
  - 验证 FQC 格式读写基本功能
  - 如有问题请询问用户

---

## Phase 2: Spring 核心算法集成 (**高风险阶段**)

- [ ] 7. Spring 代码分析与提取
  - [ ] 7.1 分析 Spring 源码结构
    - 研究 `ref-projects/Spring/src/` 目录结构
    - 识别核心模块：Minimizer Bucketing, Reordering, Consensus/Delta, Arithmetic Coding
    - 记录依赖关系和接口边界
    - **重点关注**: 全局状态依赖、内存分配模式
    - _Requirements: 1.1.2_

  - [ ] 7.2 提取 Spring 核心算法
    - 创建 `vendor/spring-core/` 目录
    - 提取 Minimizer Bucketing 相关代码
    - 提取 Reordering 算法代码
    - 提取 Consensus/Delta 编码代码
    - 移除不需要的依赖 (Boost 等)
    - 审核并记录 Spring License 约束，确保与项目发布目标一致（非商用/自用学习）
    - _Requirements: 1.1.2, 1.1.3_

- [ ] 8. 两阶段压缩策略实现 (**核心任务**)
  - [ ] 8.1 实现 Phase 1: 全局分析模块
    - 创建 `include/fqc/algo/global_analyzer.h`
    - 实现 Minimizer 提取与索引构建
    - 实现全局 Bucketing
    - 实现全局 Reordering (Approximate Hamiltonian Path)
    - 生成 Reorder Map: `original_id -> archive_id`
    - 内存估算: ~24 bytes/read
    - _Requirements: 1.1.2, 4.3_

  - [ ] 8.2 实现 Reorder Map 存储
    - 创建 `include/fqc/format/reorder_map.h`
    - 实现双向映射设计:
        - Forward Map: `original_id -> archive_id` (用于查询)
        - Reverse Map: `archive_id -> original_id` (用于原始顺序输出)
    - 实现 Delta + Varint 压缩编码
    - 实现 Reorder Map 读写
    - 目标压缩率: ~4 bytes/read (两个映射各 ~2 bytes/read)
    - _Requirements: 2.1_

  - [ ] 8.3 实现 Phase 2: 分块压缩模块
    - 创建 `include/fqc/algo/block_compressor.h`
    - 实现 Block 内 Consensus 生成
    - 实现 Block 内 Delta 编码
    - 集成算术编码器
    - 确保 Block 状态完全隔离（支持独立解压）
    - _Requirements: 1.1.2, 2.1_

  - [ ] 8.4 实现内存管理模块
    - 创建 `include/fqc/common/memory_budget.h`
    - 实现内存预算计算与监控
    - 实现超大文件分治策略 (Chunk-wise)
    - 提供 `--memory-limit` 参数支持
    - _Requirements: 4.3_

  - [ ] 8.5 编写两阶段压缩属性测试
    - **Property 3: 序列压缩往返一致性**
    - *For any* 有效的 DNA 序列集合，压缩后解压应产生等价序列
    - **Property 3.1: Reorder Map 往返一致性**
    - *For any* 重排序映射，存储后读取应产生等价映射
    - **Validates: Requirements 1.1.2, 2.1**

- [ ] 9. 质量值压缩实现
  - [ ] 9.1 实现无损质量压缩
    - 创建 `include/fqc/algo/quality_compressor.h`
    - 实现 SCM 上下文模型 (参考 Fqzcomp5，Order-1/Order-2)
    - 集成算术编码器
    - _Requirements: 3.1_

  - [ ] 9.2 实现 Illumina 8-bin 有损压缩
    - 实现质量值分箱映射表
    - 实现分箱后的压缩
    - _Requirements: 3.3_

  - [ ] 9.3 实现 QVZ 有损压缩 (可选)
    - 研究 QVZ 算法
    - 实现基于模型的有损压缩
    - _Requirements: 3.2_

  - [ ] 9.4 编写质量压缩属性测试
    - **Property 4: 无损质量压缩往返一致性**
    - *For any* 有效的质量值序列，无损压缩后解压应产生等价数据
    - **Validates: Requirements 3.1**

- [ ] 10. ID 流压缩实现
  - [ ] 10.1 实现 ID Tokenizer
    - 创建 `include/fqc/algo/id_compressor.h`
    - 实现 Illumina Header 解析和分词
    - 识别静态/动态部分
    - _Requirements: 1.1.2_

  - [ ] 10.2 实现 Delta 编码
    - 实现整数部分的 Delta 编码
    - 集成通用压缩器 (LZMA/Zstd)
    - _Requirements: 1.1.2_

  - [ ] 10.3 编写 ID 压缩属性测试
    - **Property 5: ID 压缩往返一致性**
    - *For any* 有效的 FASTQ ID 序列，压缩后解压应产生等价 ID
    - **Validates: Requirements 1.1.2**

- [ ] 11. Checkpoint - Phase 2 验证
  - 确保所有算法模块测试通过
  - 验证 Spring 适配器正确性
  - 与原始 Spring 输出对比验证
  - 如有问题请询问用户

---

## Phase 3: TBB 并行流水线

- [ ] 12. 流水线架构实现
  - [ ] 12.1 设计流水线接口
    - 创建 `include/fqc/pipeline/pipeline.h`
    - 定义 `IPipelineStage` 接口
    - 定义 `PipelineConfig` 配置结构
    - _Requirements: 4.1_

  - [ ] 12.2 实现 ReaderFilter (Serial)
    - 创建 `src/pipeline/reader_filter.cpp`
    - 实现分块读取 FASTQ
    - 配置 Block 大小:
        - Short Read: 100,000 reads/block
        - Medium Read: 50,000 reads/block
        - Long Read: 10,000 reads/block
    - 实现内存池管理
    - _Requirements: 4.1, 4.3_

  - [ ] 12.3 实现 CompressFilter (Parallel)
    - 创建 `src/pipeline/compress_filter.cpp`
    - 集成 Spring 适配器
    - 集成质量压缩器
    - 集成 ID 压缩器
    - 实现 Block 级别并行
    - _Requirements: 4.1_

  - [ ] 12.4 实现 WriterFilter (Serial)
    - 创建 `src/pipeline/writer_filter.cpp`
    - 集成 FQCWriter
    - 实现有序写入 (保持 Block 顺序)
    - _Requirements: 4.1_

- [ ] 13. 压缩引擎实现
  - [ ] 13.1 实现 CompressionEngine
    - 创建 `include/fqc/engine/compression_engine.h`
    - 创建 `src/engine/compression_engine.cpp`
    - 组装 TBB parallel_pipeline
    - 实现进度报告
    - _Requirements: 4.1_

  - [ ] 13.2 实现内存管理
    - 实现内存使用上限控制
    - 实现 Block 缓冲池
    - 监控内存使用
    - _Requirements: 4.3_

  - [ ] 13.3 编写压缩引擎属性测试
    - **Property 6: 完整压缩往返一致性**
    - *For any* 有效的 FASTQ 文件，压缩后解压应产生等价文件
    - **Validates: Requirements 1.1, 2.1, 2.2**

- [ ] 14. 解压引擎实现
  - [ ] 14.1 实现 DecompressionEngine
    - 创建 `include/fqc/engine/decompression_engine.h`
    - 创建 `src/engine/decompression_engine.cpp`
    - 实现全文件解压
    - 实现范围解压 (随机访问)
    - 实现 Header-only 解压：跳过 Sequence/Quality 解码
    - 实现子流选择性解码：可只解码 Sequence 或 Quality 子流
    - _Requirements: 2.1, 2.2, 2.3_

  - [ ] 14.2 实现并行解压
    - 使用 TBB parallel_for 并行解压多个 Block
    - 实现有序输出合并
    - _Requirements: 4.1_

- [ ] 15. Checkpoint - Phase 3 验证
  - 确保流水线测试通过
  - 验证多线程正确性
  - 性能基准测试
  - 如有问题请询问用户

---

## Phase 4: 优化与扩展

- [ ] 16. IO 优化 (Pigz 思想)
  - [ ] 16.1 实现异步 IO
    - 使用 TBB 或 std::async 实现异步读写
    - 实现双缓冲策略
    - _Requirements: 4.1_

  - [ ] 16.2 外部封装与分发建议（文档）
    - 在 README/帮助信息中说明：不提供 `.fqc.gz` 内置封装（会破坏随机访问），如需分发可由用户在归档外部自行二次压缩
    - _Requirements: 6.3_

- [ ] 17. 扩展输入格式支持 (Phase 2)
  - [ ] 17.1 实现 bzip2 输入支持
    - 集成 libbz2
    - 扩展 CompressedStream
    - _Requirements: 1.1.1_

  - [ ] 17.2 实现 xz 输入支持
    - 集成 liblzma
    - 扩展 CompressedStream
    - _Requirements: 1.1.1_

- [ ] 18. 长读支持
  - [ ] 18.1 实现长读检测
    - 采样前 1000 条 Reads 计算 median 和 max length
    - 三级分类阈值 (优先级从高到低):
        1. max_length >= 10KB → LONG
        2. max_length > 511 → MEDIUM (Spring 兼容保护，即使 median < 1KB)
        3. median >= 1KB → MEDIUM
        4. 其余 → SHORT
    - 支持 `--long-read-mode <auto|short|medium|long>` 参数覆盖
    - _Requirements: 1.1.3_

  - [ ] 18.2 实现长读压缩策略
    - **Short (max <= 511)**: ABC + 全局 Reordering，Block Size 100K
    - **Medium (max > 511 或 1KB <= median < 10KB)**: Zstd fallback (推荐)，禁用 Reordering，Block Size 50K
    - **Long (median >= 10KB)**: **Zstd (推荐首选)**，禁用 Reordering，Block Size 10K
    - Quality: Long Read 使用 SCM Order-1（降低内存）
    - **关键约束**: Spring ABC 仅用于 max_length <= 511 的数据，超过则必须使用 Zstd
    - **可选优化**: Overlap-based 压缩可作为 Phase 5+ 优化项
    - _Requirements: 1.1.3_

  - [ ] 18.3 编写长读属性测试
    - **Property 7: 长读压缩往返一致性**
    - *For any* 有效的长读序列，压缩后解压应产生等价数据
    - **Validates: Requirements 1.1.3**

- [ ] 19. Paired-End 支持
  - [ ] 19.1 实现 PE 文件处理
    - 支持双文件输入 (`-1, -2`)
    - 支持交错格式输入 (`--interleaved`)
    - 实现 PE 布局选择 (`--pe-layout <interleaved|consecutive>`)
    - _Requirements: 1.1.3_

  - [ ] 19.2 实现 PE 压缩优化
    - 利用 R1/R2 互补性：存储 R2 与 R1 反向互补的差异
    - Reordering 时保持配对关系：同时移动 (R1_i, R2_i)
    - _Requirements: 1.1.3_

  - [ ] 19.3 实现 PE 解压功能
    - 支持 `--split-pe` 分离输出 R1/R2
    - 支持交错格式输出
    - _Requirements: 2.2_

  - [ ] 19.4 编写 PE 属性测试
    - **Property 8: PE 压缩往返一致性**
    - *For any* 有效的 PE 数据，压缩后解压应产生等价配对数据
    - **Validates: Requirements 1.1.3**

- [ ] 20. Checkpoint - Phase 4 验证
  - 确保扩展功能测试通过
  - 验证各种输入格式
  - 如有问题请询问用户

---

## Phase 5: 验证与发布

- [ ] 21. 完整性验证
  - [ ] 21.1 实现 Verify 命令完整功能
    - 实现全局校验和验证
    - 实现 Block 级别校验
    - 报告损坏位置和数量
    - 支持 `--fail-fast` 选项
    - _Requirements: 5.1, 5.2, 5.3, 8.5_

  - [ ] 21.2 实现错误隔离机制
    - 确保单个 Block 损坏不影响其他 Block 解压
    - 实现 `--skip-corrupted` 功能
    - 实现损坏 Block 占位符/跳过逻辑
    - _Requirements: 8.5_

  - [ ] 21.3 实现与 Spring 对比验证
    - 创建对比测试脚本
    - 验证压缩率
    - 验证解压正确性
    - _Requirements: 1.1.2_

- [ ] 22. 集成测试
  - [ ] 22.1 编写端到端测试
    - 测试完整压缩-解压流程
    - 测试各种选项组合
    - 测试边界条件
    - _Requirements: 1.1, 2.1, 2.2, 2.3, 3.1, 3.2, 3.3, 3.4_

  - [ ] 22.2 编写性能测试
    - 测试不同文件大小
    - 测试不同线程数
    - 记录压缩率和速度
    - _Requirements: 4.1_

- [ ] 23. 文档与发布
  - [ ] 23.1 编写 README.md
    - 项目介绍
    - 安装说明
    - 使用示例
    - _Requirements: 6.3_

  - [ ] 23.2 编写 API 文档
    - 使用 Doxygen 生成文档
    - 记录公共接口
    - _Requirements: 7.3_

- [ ] 24. Final Checkpoint - 项目完成
  - 确保所有测试通过
  - 确保文档完整
  - 如有问题请询问用户

---

## Notes

- 每个 Checkpoint 是验证点，确保阶段性成果可用
- Phase 1-3 为核心功能，Phase 4-5 为优化和完善
- 属性测试使用 RapidCheck 框架，最少运行 100 次迭代
- 所有代码遵循 C++20 标准和 fastq-tools 代码风格
- 所有属性测试任务均为必须完成项

---

## Appendix A: 属性测试生成器规范

为确保属性测试的有效性，定义以下数据生成器规范：

### A.1 DNA 序列生成器
```cpp
// 参数
size_t min_length = 50;    // 最小长度
size_t max_length = 500;   // 最大长度 (Short Read)
double n_ratio = 0.01;     // N 碱基比例

// 碱基分布
// A: 25%, C: 25%, G: 25%, T: 25%, N: 1% (可配置)
```

### A.2 质量值生成器
```cpp
// Phred33 编码: ASCII 33-126 ('!' to '~')
// 实际范围: 0-93 (通常 0-41 for Illumina)
char min_qual = '!';  // Phred 0
char max_qual = 'J';  // Phred 41

// 分布: 模拟真实数据的质量值下降趋势
// 前 20%: 高质量 (30-41)
// 中 60%: 中等质量 (20-35)
// 后 20%: 低质量 (10-25)
```

### A.3 Read ID 生成器
```cpp
// Illumina 格式: @<instrument>:<run>:<flowcell>:<lane>:<tile>:<x>:<y>
// 示例: @SIM:1:FCX:1:1:1:1

// 生成策略:
// - instrument: 固定 "SIM"
// - run: 1-10
// - flowcell: "FCX"
// - lane: 1-8
// - tile: 1-100
// - x, y: 1-10000 (Delta 编码友好)
```

### A.4 Long Read 生成器
```cpp
size_t min_length = 1000;   // 最小长度
size_t max_length = 50000;  // 最大长度
double error_rate = 0.05;   // 模拟错误率 (Nanopore ~5%)
```

---

## Appendix B: 原子写入实现参考

```cpp
// include/fqc/io/atomic_writer.h
namespace fqc::io {

class AtomicFileWriter {
public:
    explicit AtomicFileWriter(std::filesystem::path target)
        : target_(std::move(target))
        , temp_(target_.string() + ".tmp")
    {
        // 注册信号处理器
        register_cleanup_handler();
    }
    
    ~AtomicFileWriter() {
        if (!committed_) {
            cleanup();
        }
    }
    
    void commit() {
        stream_.close();
        std::filesystem::rename(temp_, target_);
        committed_ = true;
    }
    
private:
    void cleanup() {
        std::filesystem::remove(temp_);
    }
    
    std::filesystem::path target_;
    std::filesystem::path temp_;
    std::ofstream stream_{temp_, std::ios::binary};
    bool committed_ = false;
};

}  // namespace fqc::io
```


---

## Appendix C: 流式模式实现参考

流式模式用于处理 stdin 输入或显式禁用全局重排序的场景。

### C.1 流式模式检测逻辑
```cpp
bool should_use_streaming_mode(const CompressOptions& opts) {
    // 显式指定流式模式
    if (opts.streaming) return true;
    
    // stdin 输入自动启用流式模式
    if (opts.input == "-") {
        LOG_WARNING("stdin input detected, enabling streaming mode (no global reordering)");
        return true;
    }
    
    return false;
}
```

### C.2 流式模式压缩流程
```
Streaming Mode Compression:
├── 1. 读取 Block 大小的 Reads
├── 2. Block 内局部优化 (可选的 Block 内重排序)
├── 3. 压缩并写入 Block
├── 4. 重复直到 EOF
└── 5. 写入 Index 和 Footer

注意:
- 跳过 Phase 1 全局分析
- PRESERVE_ORDER=1 或 STREAMING_MODE=1
- 压缩率降低约 10-20%
- 内存使用显著降低
```

### C.3 性能对比预期
| 模式 | 压缩率 (bits/base) | 内存使用 | 适用场景 |
|------|-------------------|----------|----------|
| 标准模式 | 0.4-0.6 | ~24 bytes/read | 文件输入 |
| 流式模式 | 0.5-0.8 | ~50 bytes/block | stdin/管道 |

---

## Appendix D: Read 长度分类参考

### D.1 分类阈值
| 分类 | Median Length | 典型数据类型 |
|------|---------------|--------------|
| Short | < 1KB 且 max <= 511 | Illumina (50-300bp) |
| Medium | 1KB - 10KB 或 max > 511 | PacBio HiFi, Illumina Long |
| Long | >= 10KB | Nanopore, PacBio CLR |

### D.2 各分类压缩策略
```cpp
struct CompressionStrategy {
    ReadLengthClass length_class;
    bool enable_global_reorder;
    size_t block_size;
    uint8_t quality_scm_order;
    uint8_t sequence_codec;
};

const CompressionStrategy STRATEGIES[] = {
    // Short (max <= 511): Spring ABC + 全局 Reordering
    {SHORT, true, 100000, 2, CODEC_ABC_V1},
    // Medium (max > 511 或 1KB <= median < 10KB): Zstd fallback，禁用 Reordering
    {MEDIUM, false, 50000, 2, CODEC_ZSTD_PLAIN},
    // Long (median >= 10KB): Zstd 或 Overlap-based，禁用 Reordering
    {LONG, false, 10000, 1, CODEC_ZSTD_PLAIN},  // 或 CODEC_OVERLAP_V1
};

// 关键约束: Spring ABC (CODEC_ABC_V1) 仅用于 max_length <= 511 的数据
// 超过 511bp 必须使用 CODEC_ZSTD_PLAIN 或其他通用压缩
```
