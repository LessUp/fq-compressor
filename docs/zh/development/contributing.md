---
title: 贡献指南
description: 如何为 fq-compressor 项目做出贡献
version: 0.1.0
last_updated: 2026-04-17
language: zh
---

# 贡献指南

感谢您有兴趣为 fq-compressor 做出贡献！本文档提供了项目的贡献指南。

## 行为准则

- 在所有交流中保持尊重和建设性
- 关注技术价值和项目改进
- 帮助为新来者创造友好的环境

## 快速开始

### 开发环境设置

1. **Fork 并克隆** 仓库
2. **设置构建环境**:
   ```bash
   conan install . --build=missing -of=build/clang-debug
   cmake --preset clang-debug
   ```
3. **构建并测试**:
   ```bash
   cmake --build --preset clang-debug -j$(nproc)
   ./scripts/test.sh clang-debug
   ```

### DevContainer（推荐）

为了获得一致的开发环境：

1. 在 VS Code 中打开项目
2. 运行 `Dev Containers: Reopen in Container`
3. 所有工具已预装

## 开发工作流

### 分支命名

- `feature/description` - 新功能
- `fix/description` - Bug 修复
- `docs/description` - 文档更新
- `refactor/description` - 代码重构

### 提交信息

遵循 [Conventional Commits](https://www.conventionalcommits.org/) 规范：

```
<类型>(<范围>): <描述>

[可选正文]

[可选页脚]
```

类型：
- `feat` - 新功能
- `fix` - Bug 修复
- `docs` - 仅文档更改
- `style` - 格式更改
- `refactor` - 代码重构
- `perf` - 性能改进
- `test` - 测试更新
- `chore` - 构建/工具更改

示例：
```
feat(compressor): add quality score binning option
fix(parser): handle empty FASTQ files
docs(readme): update installation instructions
```

### 代码风格

项目使用自动格式化和静态检查：

```bash
# 格式化代码
./scripts/lint.sh format

# 运行静态检查
./scripts/lint.sh lint clang-debug

# 检查所有
./scripts/lint.sh all clang-debug
```

风格指南：
- 4 空格缩进
- 100 列行宽限制
- 文件名 snake_case，类型名 PascalCase，函数名 camelCase
- 重要返回值使用 `[[nodiscard]]`
- 不使用裸指针管理资源

## 提交更改

### Pull Request 流程

1. **创建分支** 从 `main`
2. **进行更改** 并编写清晰的提交信息
3. **确保测试通过**:
   ```bash
   ./scripts/test.sh clang-release
   ```
4. **更新文档**（如需要）
5. **提交 PR** 并附上清晰的描述

### PR 检查清单

- [ ] 代码编译无警告
- [ ] 所有测试通过
- [ ] 新功能添加了新测试
- [ ] 文档已更新
- [ ] 更新日志已更新（如适用）
- [ ] 代码已用 clang-format 格式化
- [ ] 无 clang-tidy 警告

### 审核流程

1. 维护者将在 48 小时内审核
2. 及时响应审核反馈
3. 如要求则压缩提交
4. 批准后，维护者将合并

## 测试指南

### 单元测试

使用 Google Test：

```cpp
#include <gtest/gtest.h>

TEST(CompressionTest, BasicCompression) {
    // 准备
    Compressor comp;

    // 执行
    auto result = comp.compress(data);

    // 断言
    EXPECT_TRUE(result.has_value());
    EXPECT_LT(result->size(), data.size());
}
```

### 属性测试

当 RapidCheck 可用时使用：

```cpp
RC_GTEST_PROP(CompressorTest, RoundTrip, ()) {
    auto data = *rc::gen::container<std::vector<uint8_t>>(
        rc::gen::inRange(0, 255)
    );

    auto compressed = compress(data);
    auto restored = decompress(compressed);

    RC_ASSERT(restored == data);
}
```

### 添加测试

在 `tests/` 中添加测试到适当的文件：

```cmake
# 在 tests/CMakeLists.txt 中
fqc_add_test(my_feature_test src/my_feature_test.cpp)
```

## 性能考虑

修改压缩代码时：

1. **前后性能基准测试**:
   ```bash
   python3 benchmark/compiler_benchmark.py \
       --input test_data.fq \
       --gcc-binary build/gcc-release/src/fqc \
       --visualize
   ```

2. **内存分析**:
   ```bash
   valgrind --tool=massif ./fqc compress -i large.fastq -o out.fqc
   ```

3. **目标指标**:
   - 压缩比：保持或改进
   - 速度：可接受最小回退
   - 内存：保持在预算系统限制内

## 文档

### 代码文档

公共 API 使用 Doxygen 风格注释：

```cpp
/**
 * 将 FASTQ 文件压缩为 FQC 格式。
 *
 * @param input 输入 FASTQ 文件路径
 * @param output 输出 FQC 文件路径
 * @param options 压缩配置
 * @return 表示成功或错误详情的 Result
 *
 * @throws FQCException 遇到不可恢复的错误时
 */
Result<void> compressFile(const std::filesystem::path& input,
                          const std::filesystem::path& output,
                          const CompressOptions& options);
```

### 用户文档

更新相关文档：
- CLI 更改 → `docs/zh/getting-started/cli-usage.md`
- 算法更改 → `docs/zh/core-concepts/algorithms.md`
- 格式更改 → `docs/zh/core-concepts/fqc-format.md`
- 破坏性更改 → `CHANGELOG.md`

## 贡献领域

### 适合新手的任务

- 文档改进
- 增加测试覆盖
- 错误信息改进
- 日志信息优化

### 高级贡献

- 算法优化
- 新压缩模式
- 平台支持扩展
- 工具改进

## 有问题？

- 在 [讨论区](https://github.com/LessUp/fq-compressor/discussions) 提问
- 加入社区聊天（即将推出）

## 许可证

通过贡献，您同意您的贡献将按照 MIT 许可证授权。

---

感谢您为 fq-compressor 做出贡献！
