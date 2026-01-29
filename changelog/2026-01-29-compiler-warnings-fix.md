# 编译器警告修复 (2026-01-29)

## 概述
系统性修复了 GCC 和 Clang 编译器产生的所有项目代码警告，确保 GCC Release 构建无代码警告通过。

## 修复的警告类型

### 类型转换警告 (`-Wconversion`, `-Wsign-conversion`)
- `memory_budget.cpp`: `std::size_t` 到 `double` 的显式转换
- `pipeline.cpp`: `std::chrono::duration` 到 `uint64_t` 的显式转换
- `fastq_parser.h`: `ParserStats::averageLength()` 中的类型转换
- `compress_command.h/cpp`: 多处 `size_t` 和 `int` 到 `double` 的转换
- `decompress_command.cpp`: 进度回调中的类型转换
- `quality_compressor.cpp`: `ArithmeticDecoder` 中的符号转换
- `pipeline_node.cpp`: 文件大小估算中的类型转换
- `pe_optimizer.cpp`: `std::abs()` 返回值的符号转换
- `memory_budget_test.cpp`: 测试代码中的类型转换

### Switch 语句警告 (`-Wswitch`)
- `error.cpp`: `toException()` 和 `throwException()` 函数中添加所有 `ErrorCode` 枚举值的处理

### 未使用代码警告 (`-Wunused-*`)
- `main.cpp`: 删除未使用的 `isStdinTty()` 函数
- `id_compressor.cpp`: 移除 `IDTokenizer::tryParseInt()` 中未使用的 `negative` 变量
- `memory_budget_test.cpp`: 添加 `[[maybe_unused]]` 属性

### ODR 违规警告 (`-Wodr`, `-Wlto-type-mismatch`)
- 将 `fqc_reader.h` 中的 `ReorderMapData` 结构重命名为 `LoadedReorderMapData`
- 避免与 `reorder_map.h` 中同名类型冲突

### Clang 特有警告 (`-Wshadow-field-in-constructor`)
- `types.h`: 重命名 `ReadRecord` 构造函数参数避免遮蔽成员变量
- `id_compressor.h`: 修复 `zigzagDecode()` 中的符号转换

## 修改的文件列表

| 文件 | 修改类型 |
|------|----------|
| `src/common/error.cpp` | 补全 switch 枚举处理 |
| `src/main.cpp` | 删除未使用函数 |
| `src/common/memory_budget.cpp` | 添加显式类型转换 |
| `src/pipeline/pipeline.cpp` | 添加显式类型转换 |
| `src/pipeline/pipeline_node.cpp` | 添加显式类型转换 |
| `src/algo/id_compressor.cpp` | 移除未使用变量 |
| `src/algo/quality_compressor.cpp` | 修复符号转换 |
| `src/algo/pe_optimizer.cpp` | 修复符号转换 |
| `src/commands/compress_command.cpp` | 添加显式类型转换 |
| `src/commands/decompress_command.cpp` | 添加显式类型转换 |
| `src/format/fqc_reader.cpp` | 更新类型名称引用 |
| `include/fqc/io/fastq_parser.h` | 添加显式类型转换 |
| `include/fqc/format/fqc_reader.h` | 重命名 `ReorderMapData` |
| `include/fqc/common/types.h` | 重命名构造函数参数 |
| `include/fqc/algo/id_compressor.h` | 修复 `zigzagDecode` |
| `src/commands/compress_command.h` | 添加显式类型转换 |
| `tests/common/memory_budget_test.cpp` | 添加类型转换和属性 |

## 验证结果
- ✅ GCC Release 构建成功（无项目代码警告）
- ⚠️ 仅剩 quill 第三方库的 LTO 警告（代码外部问题）

## 备注
quill 日志库在 LTO 优化时产生的 `-Wstringop-overread` 警告来自库内部代码，无法在项目中修复。建议：
1. 等待 quill 库更新修复
2. 或考虑使用 `-Wno-stringop-overread` 编译选项抑制此特定警告
