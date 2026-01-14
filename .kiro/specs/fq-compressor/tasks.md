# Implementation Plan: fq-compressor

## Overview

本实现计划将 fq-compressor 项目分为 5 个主要阶段，按照设计文档中的 Development Phases 组织。每个阶段包含可独立验证的任务，确保增量开发和持续集成。

实现语言：**C++20**
构建系统：**CMake 3.20+**
依赖管理：**Conan 2.x**

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
    - _Requirements: 2.1, 5.1, 5.2_

  - [ ] 3.3 实现 FQCReader 类
    - 创建 `include/fqc/format/fqc_reader.h` 和 `src/format/fqc_reader.cpp`
    - 实现 `read_global_header()`, `read_block()`, `read_index()`
    - 实现随机访问：`seek_to_block()`, `get_reads_range()`
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
    - 实现全局选项：`-t/--threads`, `-v/--verbose`
    - _Requirements: 6.1, 6.2, 6.3_

  - [ ] 5.2 实现 CompressCommand 框架
    - 创建 `src/commands/compress_command.h/cpp`
    - 解析压缩相关选项：`-i`, `-o`, `-l`, `--reorder`, `--lossy-quality`
    - 预留压缩引擎调用接口
    - _Requirements: 6.2_

  - [ ] 5.3 实现 DecompressCommand 框架
    - 创建 `src/commands/decompress_command.h/cpp`
    - 解析解压选项：`-i`, `-o`, `--range`, `--header-only`
    - 预留解压引擎调用接口
    - _Requirements: 6.2_

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

## Phase 2: Spring 核心算法集成

- [ ] 7. Spring 代码分析与提取
  - [ ] 7.1 分析 Spring 源码结构
    - 研究 `ref-projects/Spring/src/` 目录结构
    - 识别核心模块：Minimizer Bucketing, Reordering（可选）, Consensus/Delta, Arithmetic Coding（Quality Compression 仅作对照参考）
    - 记录依赖关系和接口边界
    - _Requirements: 1.1.2_

  - [ ] 7.2 提取 Spring 核心算法
    - 创建 `vendor/spring-core/` 目录
    - 提取 Minimizer Bucketing 相关代码
    - 提取 Reordering 算法代码
    - 提取 Consensus/Delta 编码代码
    - 移除不需要的依赖 (Boost 等)
    - 审核并记录 Spring License 约束，确保与项目发布目标一致（非商用/自用学习）
    - _Requirements: 1.1.2, 1.1.3_

- [ ] 8. Spring 算法适配层
  - [ ] 8.1 设计 Block-aware 接口
    - 创建 `include/fqc/algo/spring_adapter.h`
    - 定义 `ISequenceCompressor` 接口 (Concept)
    - 设计 `ResetContext()` 方法支持 Block 边界重置
    - _Requirements: 1.1.2, 2.1_

  - [ ] 8.2 实现 SpringABC Bucketing/Reordering 适配器
    - 创建 `src/algo/spring_reorder.cpp`
    - 封装 Minimizer Bucketing
    - 封装 Approximate Hamiltonian Path 重排序
    - 实现 Block 级别的状态重置
    - _Requirements: 1.1.2_

  - [ ] 8.3 实现 SpringABC Encoder 适配器
    - 创建 `src/algo/spring_encoder.cpp`
    - 封装 Consensus 生成
    - 封装 Delta 编码
    - 集成算术编码器
    - _Requirements: 1.1.2_

  - [ ] 8.4 编写序列压缩属性测试
    - **Property 3: 序列压缩往返一致性**
    - *For any* 有效的 DNA 序列集合，压缩后解压应产生等价序列
    - **Validates: Requirements 1.1.2**

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
    - 集成通用压缩器 (LZMA)
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
    - 配置 Block 大小 (默认 10000 reads)
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

  - [ ] 16.2 实现输出压缩
    - 支持输出为 .fqc.gz (可选)
    - 集成 libdeflate 高性能压缩
    - _Requirements: 1.1.1_

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
    - 自动检测 read 长度分布
    - 切换压缩策略
    - _Requirements: 1.1.3_

  - [ ] 18.2 实现长读压缩策略
    - 禁用重排序或使用专有算法
    - 优化质量值压缩
    - _Requirements: 1.1.3_

- [ ] 19. Paired-End 支持
  - [ ] 19.1 实现 PE 文件处理
    - 支持双文件输入 (-1, -2)
    - 支持交错格式
    - _Requirements: 1.1.3_

  - [ ] 19.2 实现 PE 压缩优化
    - 利用配对信息优化压缩
    - 保持配对关系
    - _Requirements: 1.1.3_

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
    - 报告损坏位置
    - _Requirements: 5.1, 5.2, 5.3_

  - [ ] 21.2 实现与 Spring 对比验证
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
