# fq-compressor 编码规范

## 概述

本文档定义了 fq-compressor 项目的编码规范和最佳实践。规范基于 fastq-tools 项目的成熟实践，并结合业界主流标准（Google C++ Style Guide、LLVM Style）进行优化。

### 设计原则

1. **内部一致性优先**：项目内部风格统一比符合某个外部标准更重要
2. **工具强制执行**：所有风格规则通过 clang-format 和 clang-tidy 自动检查
3. **现代 C++ 优先**：充分利用 C++20 特性，避免过时的 C++ 习惯

## 基本规则

| 项目 | 规范 |
|------|------|
| **语言标准** | C++20 |
| **编译器** | GCC 14+ 或 Clang 19+（推荐与容器/CI 对齐） |
| **格式化工具** | clang-format 19+（以 `.clang-format` 为准） |
| **静态分析** | clang-tidy 19+（以 `.clang-tidy` 为准） |
| **缩进** | 4 空格（禁止 Tab） |
| **行宽** | 100 字符 |
| **文件编码** | UTF-8（无 BOM） |
| **换行符** | LF（Unix 风格） |

## 命名约定

### 总览表

| 元素 | 风格 | 示例 |
|------|------|------|
| **文件名（代码）** | `snake_case` | `block_encoder.h`, `fqc_writer.cpp` |
| **文件名（文档）** | `kebab-case` | `coding-standards.md` |
| **目录名** | `snake_case` | `src/format/`, `include/fqc/` |
| **类/结构体** | `PascalCase` | `class BlockEncoder;` |
| **函数/方法** | `camelCase` | `void encodeBlock();` |
| **变量（局部/参数）** | `camelCase` | `int blockSize = 0;` |
| **成员变量** | `camelCase_` | `std::string filePath_;` |
| **常量** | `kCamelCase` | `constexpr int kMaxBlockSize = 100000;` |
| **枚举类型** | `CamelCase` | `enum class Mode { Compress, Decompress };` |
| **枚举值** | `CamelCase` | `enum class Mode { Compress, Decompress };` |
| **宏** | `SCREAMING_SNAKE_CASE` | `#define FQC_VERSION "1.0.0"` |
| **命名空间** | `snake_case` | `namespace fqc::format` |
| **模板参数** | `CamelCase` | `template <typename InputIterator>` |

### 命名规则详解

#### 1. 类和结构体
```cpp
// ✅ 正确
class BlockEncoder;
struct GlobalHeader;
class FQCWriter;  // 缩写保持大写

// ❌ 错误
class block_encoder;
class blockEncoder;
```

#### 2. 函数和方法
```cpp
// ✅ 正确
auto encodeBlock(const Block& block) -> EncodedData;
void setInputPath(const std::string& path);
auto isValid() const -> bool;

// ❌ 错误
auto EncodeBlock();  // 不要用 PascalCase
auto encode_block(); // 不要用 snake_case
```

#### 3. 变量
```cpp
// ✅ 正确
int blockCount = 0;
std::string inputPath;
auto& currentBlock = blocks[i];

// ❌ 错误
int block_count;  // 不要用 snake_case
int BlockCount;   // 不要用 PascalCase
```

#### 4. 成员变量
```cpp
class BlockEncoder {
private:
    std::string inputPath_;      // ✅ 尾部下划线
    size_t blockSize_;
    std::vector<Block> blocks_;
    
    // ❌ 错误
    std::string m_inputPath;     // 不要用 m_ 前缀
    std::string _inputPath;      // 不要用前导下划线
};
```

#### 5. 常量
```cpp
// ✅ 正确
constexpr int kMaxBlockSize = 100000;
constexpr char kMagicHeader[] = "\x89FQC";
static const std::string kDefaultExtension = ".fqc";

// ❌ 错误
constexpr int MAX_BLOCK_SIZE = 100000;  // 不要用 SCREAMING_CASE
constexpr int maxBlockSize = 100000;    // 不要用普通变量风格
```

#### 6. 命名空间
```cpp
// ✅ 正确
namespace fqc::format {
namespace fqc::algo {
namespace fqc::io {

// ❌ 错误
namespace FQC::Format {  // 不要用 PascalCase
namespace fqcFormat {    // 不要用 camelCase
```

### 文件命名例外

以下文件名为行业惯例，不纳入 `snake_case` 约束：

- `README.md`, `CHANGELOG.md`, `LICENSE`, `CONTRIBUTING.md`
- `CMakeLists.txt`, `*.cmake`, `*.cmake.in`
- `Dockerfile`, `docker-compose.yml`
- `conanfile.py`, `conanfile.txt`
- `.clang-format`, `.clang-tidy`, `.editorconfig`

## 代码格式

### 头文件保护

使用 `#pragma once`（现代编译器均支持）：

```cpp
#pragma once

namespace fqc::format {
// ...
}
```

### Include 顺序

include 顺序与 `.clang-format` 对齐，避免贡献者与自动格式化冲突：

```cpp
// 1. 对应的头文件（仅在 .cpp 文件中）
#include "fqc/format/block_encoder.h"

// 2. C/C++ 标准库
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

// 3. 第三方库
#include <CLI/CLI.hpp>
#include <quill/Quill.h>
#include <tbb/parallel_pipeline.h>

// 4. 本项目头文件
#include "fqc/common/types.h"
#include "fqc/format/fqc_format.h"
```

### 函数声明

**返回类型风格**：默认使用常规返回类型；仅在模板或复杂返回类型时使用尾随返回类型（Trailing Return Type）：

```cpp
// ✅ 推荐：简单返回类型使用常规风格
void setInputPath(const std::string& path);
int getBlockCount() const;
bool isValid() const noexcept;

// ✅ 推荐：复杂/模板返回类型使用尾随语法
auto createPipeline() -> std::unique_ptr<Pipeline>;
auto encodeBlock(const Block& block) -> EncodedData;

template <typename T>
auto process(T&& value) -> decltype(auto);

// ❌ 避免：简单类型强制使用尾随语法
auto getCount() -> int;  // 过度使用
```

### 大括号风格

采用 **Attach** 风格（K&R 变体）：

```cpp
// ✅ 正确
if (condition) {
    doSomething();
} else {
    doOther();
}

for (const auto& item : items) {
    process(item);
}

class MyClass {
public:
    void method() {
        // ...
    }
};

// ❌ 错误：Allman 风格
if (condition)
{
    doSomething();
}
```

### 空行规则

```cpp
class BlockEncoder {
public:
    // 访问修饰符前后各一个空行（首个除外）
    BlockEncoder() = default;
    ~BlockEncoder() = default;

    auto encode(const Block& block) -> EncodedData;

private:
    // private 前空一行
    std::string inputPath_;
};

// 函数之间空一行
auto functionA() -> void {
    // ...
}

auto functionB() -> void {
    // ...
}
```

## 现代 C++ 实践

### 智能指针

```cpp
// ✅ 优先使用智能指针
auto encoder = std::make_unique<BlockEncoder>();
auto config = std::make_shared<Config>();

// ❌ 禁止裸 new/delete
auto* encoder = new BlockEncoder();  // 禁止
delete encoder;                       // 禁止
```

### 类型推导

```cpp
// ✅ 使用 auto 简化复杂类型
auto it = container.begin();
auto result = computeResult();
auto& ref = getReference();

// ✅ 显式类型用于提高可读性
int count = 0;
std::string name = "default";

// ❌ 避免过度使用 auto
auto x = 42;  // 不清晰，应该用 int x = 42;
```

### 范围 for 循环

```cpp
// ✅ 优先使用范围 for
for (const auto& block : blocks) {
    process(block);
}

// ✅ 需要索引时使用 enumerate（C++23）或传统 for
for (size_t i = 0; i < blocks.size(); ++i) {
    process(blocks[i], i);
}
```

### 字符串处理

```cpp
// ✅ 只读参数使用 string_view
auto processSequence(std::string_view sequence) -> bool;

// ✅ 需要所有权时使用 string
auto readFile(const std::string& path) -> std::string;

// ✅ 字符串拼接使用 format（C++20）
auto message = std::format("Block {} has {} reads", blockId, readCount);
```

### 错误处理

```cpp
// ✅ 使用异常处理可恢复错误
if (!file.is_open()) {
    throw FQCException(ErrorCode::FileNotFound, 
                       std::format("Cannot open file: {}", path));
}

// ✅ 使用 std::expected（C++23）或自定义 Result 类型
auto readBlock(size_t index) -> std::expected<Block, Error>;

// ✅ 使用 [[nodiscard]] 防止忽略返回值
[[nodiscard]] auto isValid() const -> bool;
```

### Concepts 约束

```cpp
// ✅ 使用 Concepts 约束模板
template <std::ranges::input_range R>
auto processRange(R&& range) -> void;

template <typename T>
    requires std::is_integral_v<T>
auto encode(T value) -> std::vector<uint8_t>;
```

## 注释规范

### 文件头注释

```cpp
/**
 * @file block_encoder.h
 * @brief Block 编码器接口定义
 * 
 * @author fq-compressor Team
 * @date 2026
 * @copyright MIT License
 */
```

### API 文档

```cpp
/**
 * @brief 编码单个 Block
 * 
 * @param block 要编码的 Block 数据
 * @param config 编码配置
 * @return 编码后的数据
 * @throws EncodingError 编码失败时抛出
 * 
 * @note 此函数是线程安全的
 * @see decodeBlock()
 */
auto encodeBlock(const Block& block, const Config& config) -> EncodedData;
```

### 行内注释

```cpp
// ✅ 解释"为什么"，而非"是什么"
// 使用 xxHash64 而非 CRC32，因为它在大数据量下更快
auto checksum = xxhash64(data);

// ❌ 避免显而易见的注释
int count = 0;  // 初始化 count 为 0（无意义）
```

### TODO 注释

```cpp
// TODO(username): 描述待办事项
// FIXME(username): 描述需要修复的问题
// HACK(username): 描述临时解决方案
```

## 目录结构

```
fq-compressor/
├── include/fqc/           # 公共 API 头文件
│   ├── common/            # 通用工具
│   ├── format/            # 文件格式
│   ├── algo/              # 压缩算法
│   ├── io/                # 输入输出
│   ├── pipeline/          # 处理流水线
│   └── fqc.h              # 聚合头文件
├── src/                   # 实现代码
│   ├── common/
│   ├── format/
│   ├── algo/
│   ├── io/
│   ├── pipeline/
│   ├── commands/          # CLI 命令实现
│   └── main.cpp           # 程序入口
├── tests/                 # 测试代码
│   ├── unit/              # 单元测试
│   ├── property/          # 属性测试
│   └── integration/       # 集成测试
├── vendor/                # 第三方代码（如 Spring Core）
├── docs/                  # 文档
│   ├── dev/               # 开发文档
│   └── user/              # 用户文档
├── scripts/               # 构建/测试脚本
├── cmake/                 # CMake 模块
└── docker/                # 容器配置
```

## 工具配置

### clang-format 规则说明

- include 分组顺序：标准库 → 第三方 → 项目内。
- 自动格式化不应被手动覆盖，提交前务必运行格式化脚本。

### clang-tidy 规则说明

- 启用：clang-diagnostic / clang-analyzer / bugprone / cppcoreguidelines / misc / modernize / performance / portability / readability。
- 默认禁用：
  - `modernize-use-trailing-return-type`（不强制尾随返回类型）
  - `bugprone-easily-swappable-parameters`
  - `cppcoreguidelines-pro-bounds-*`
  - `misc-non-private-member-variables-in-classes`
- 命名检查：枚举/枚举值为 CamelCase，宏为 UPPER_CASE，模板参数为 CamelCase。
- 断言宏：`assert` 与 `FQTOOLS_ASSERT` 均视为断言。

### editorconfig 规则说明

- 全局默认：UTF-8、LF、4 空格缩进。
- YAML 使用 2 空格缩进；Makefile 必须使用 Tab。
- C/C++ 行宽 100；Markdown 保留行尾空格。

### 格式化命令

```bash
# 格式化所有代码
./scripts/lint.sh format

# 检查格式（不修改）
./scripts/lint.sh format-check

# 静态分析
./scripts/lint.sh lint
```

### 构建命令

```bash
# 快速构建
./scripts/build.sh

# 指定编译器和配置
./scripts/build.sh clang Release
./scripts/build.sh gcc Debug

# 启用 Sanitizers
./scripts/build.sh clang Debug --asan --usan
```

### 测试命令

```bash
# 运行所有测试
./scripts/test.sh

# 运行特定测试
./scripts/test.sh --filter "BlockEncoder*"
```

## 参考资料

- [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)
- [LLVM Coding Standards](https://llvm.org/docs/CodingStandards.html)
- [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines)
- [fastq-tools 编码规范](../ref-projects/fastq-tools/docs/dev/coding-standards.md)
