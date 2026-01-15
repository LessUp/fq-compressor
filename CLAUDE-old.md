[根目录](../CLAUDE.md) > **fq-compressor**

# fq-compressor 主体项目

> 高性能 FASTQ 压缩工具的设计与配置

## 模块职责

fq-compressor 是项目的主体目录，包含所有设计文档、构建配置和开发规范。

## 入口与启动

当前项目处于**设计阶段**，尚未开始编码实现。预期的入口点：

```bash
# 构建命令
./scripts/build.sh <compiler> <build_type>

# 测试命令
./scripts/test.sh -c <compiler> -t <build_type>

# 压缩示例 (未来)
./build/fq-compressor compress input.fastq -o output.fqc

# 解压示例 (未来)
./build/fq-compressor decompress input.fqc -o output.fastq
```

## 对外接口

### 命令行接口 (CLI11)

| 命令 | 用途 |
|------|------|
| `compress` | FASTQ 压缩 |
| `decompress` | FQC 解压 |
| `bench` | 性能基准测试 |
| `stat` | 文件统计信息 |

### 预期参数

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `--input/-i` | 输入文件 | 必填 |
| `--output/-o` | 输出文件 | 必填 |
| `--threads/-j` | 并行线程数 | CPU 核心数 |
| `--memory-limit` | 内存限制 | 无限制 |
| `--level/-l` | 压缩级别 | 2 |

## 关键依赖与配置

### 构建依赖 (Conan)

| 依赖 | 版本 | 用途 |
|------|------|------|
| cli11 | 2.4.2 | CLI 解析 |
| quill | 11.0.2 | 日志 |
| onetbb | 2022.3.0 | 并行处理 |
| zlib-ng | 2.3.2 | 压缩 |
| zstd | 1.5.7 | 压缩 |
| libdeflate | 1.25 | 压缩 |

### 构建配置

- **CMake 预设**: `CMakePresets.json`
- **编译器**: GCC 12+ / Clang 17+
- **C++ 标准**: C++20
- **代码风格**: `.clang-format`
- **静态检查**: `.clang-tidy`

## 数据模型

### FQC 格式设计

FQC 是项目的自定义压缩格式，支持随机访问：

```
┌─────────────────────────────────────────────┐
│  File Header (64 bytes)                      │
├─────────────────────────────────────────────┤
│  Block 1 (e.g., 10MB)                        │
│  ├─ Block Header (32 bytes)                  │
│  ├─ ID Stream (Delta + Tokenized)            │
│  ├─ Sequence Stream (ABC Encoded)            │
│  └─ Quality Stream (SCM Encoded)             │
├─────────────────────────────────────────────┤
│  Block 2                                     │
├─────────────────────────────────────────────┤
│  ...                                         │
├─────────────────────────────────────────────┤
│  Index Footer (Variable)                     │
│  ├─ Block Index Entries                      │
│  └─ Magic Number                             │
└─────────────────────────────────────────────┘
```

### Read 长度分类

| 类型 | 条件 | 压缩策略 |
|------|------|----------|
| **Short** | median < 1KB 且 max <= 511 | Spring ABC |
| **Medium** | 1KB <= median < 10KB 或 max > 511 | Zstd |
| **Long** | median >= 10KB | Zstd (推荐) |

## 测试与质量

### CI/CD 流程

- **Lint**: clang-format, clang-tidy, commitlint
- **Build**: GCC/Clang, Debug/Release
- **Test**: 单元测试 + 集成测试
- **Coverage**: lcov 覆盖率报告

### 编码规范

- 列限制: 100 字符
- 缩进: 4 空格
- 换行: LF
- 遵循 Google C++ Style Guide (clamped)

## 常见问题 (FAQ)

### Q: 项目目前处于什么阶段？

A: 设计阶段。所有设计文档已完成评审，尚未开始编码实现。

### Q: 为什么选择 Spring ABC 而非其他算法？

A: Spring 的 ABC 策略在短读数据上能达到接近有参考压缩的比率，同时保持无参考的灵活性。

### Q: 如何处理长读数据？

A: 长读数据（max > 511bp）会自动切换到 Zstd 压缩策略，因为 Spring ABC 对长读收益有限。

### Q: 项目许可证是什么？

A: 计划使用 MIT 许可证，但 Spring 依赖可能导致非商业限制（待最终确认）。

## 相关文件清单

### 设计文档

- `docs/01_feasibility_analysis.md` - 可行性分析
- `docs/02_strategy_evaluation.md` - 策略评估
- `docs/03_algorithm_selection.md` - 算法选择
- `docs/reivew/*.md` - 设计评审报告

### 构建配置

- `CMakePresets.json` - CMake 预设
- `conanfile.py` - Conan 依赖配置
- `.clang-format` - 代码格式
- `.clang-tidy` - 静态检查
- `.github/workflows/ci.yml` - CI/CD

### 开发脚本

- `scripts/build.sh` - 构建脚本
- `scripts/test.sh` - 测试脚本
- `scripts/lint.sh` - 代码检查脚本

## 变更记录

| 日期 | 操作 | 说明 |
|------|------|------|
| 2026-01-15 | 初始化 | 创建模块文档 |
