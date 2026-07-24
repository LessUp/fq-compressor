# AGENTS.md - fq-compressor

个人练习项目：基于 C++23 的高性能 FASTQ 压缩并发流水线。

## 构建与测试

```bash
./scripts/build.sh clang-debug      # 构建
./scripts/test.sh clang-debug       # 运行全部测试
./scripts/test.sh clang-asan        # sanitizer 构建 + 测试
./scripts/lint.sh format-check      # 检查格式
./scripts/lint.sh format            # 自动格式化
```

## 代码风格

- C++23，clang + libc++，4 空格缩进，100 列限制
- 命名：类型 PascalCase，函数/变量 camelCase，成员变量 `trailing_` 后缀，常量 `kPascal`
- 错误处理：`Result<T>`（即 `std::expected<T, Error>` 的别名），库代码里不用异常
- 日志：`fqc/log.h` 中的 `FQC_LOG_INFO(...)` 等宏（基于 fmt，输出到 stderr）
- 头文件顺序：项目头文件优先，其次标准库，最后第三方库

## 架构

```
FASTQ 输入 → FastqParser → [SPSC 队列] → ArchiveWriter（编码 + zstd + xxh64）→ 输出
```

核心模块：
- `include/fqc/pipeline/` — SPSC 队列 + 流水线编排（核心学习点）
- `src/format/archive.cpp` — 二进制格式：varint、2-bit DNA 打包、zstd 帧、XXH64
- `src/io/` — FASTQ 解析、gzip 透明输入
- `src/commands/` — CLI 编排（compress/decompress/verify）

## 依赖（Conan）

cli11、fmt、zlib-ng、zstd、xxhash、gtest（仅测试）

## Git

- 提交信息使用中文
- 格式：`<类型>: <简述>`，如 `refactor: 移除 Quill 依赖`、`feat: 添加 SPSC 并发流水线`
