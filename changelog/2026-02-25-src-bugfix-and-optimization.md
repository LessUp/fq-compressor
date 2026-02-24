# 源码 Bug 修复与优化

**日期**: 2026-02-25

## 变更内容

### Bug 修复

#### 全局 CLI 选项未传递到命令实现
- `--threads` 和 `--memory-limit` 被硬编码为 0，用户指定的值完全无效
- `--no-progress` 未传递到命令的 `showProgress` 选项
- **修复**: `runCompress` 和 `runDecompress` 现在正确传递 `gOptions.threads`、`gOptions.memoryLimit`、`!gOptions.noProgress`

#### `-v` 标志无效果
- `verbosity >= 1` 设置的日志级别仍为 `kInfo`（与默认相同），只有 `-vv` 才有效
- **修复**: `verbosity >= 1` 现在正确设置为 `kDebug`

#### `verify` 命令为空壳
- `runVerify` 是一个 stub，直接返回 SUCCESS，未使用已完整实现的 `VerifyCommand` 类
- **修复**: 接入实际的 `VerifyCommand` 实现

#### compress 命令多个 CLI 选项未传递
- `input2`（PE R2 输入）、`maxBlockBases`、`scanAllLengths`、`interleaved`、`peLayout` 在 CLI 中定义但未传递到 `CompressCommand`
- **修复**: `runCompress` 现在直接构建 `CompressOptions` 并设置所有字段

### 代码质量

#### 消除重复枚举 `QualityCompressionMode`
- `compress_command.h` 定义了 `QualityCompressionMode`，与 `types.h` 中的 `QualityMode` 完全重复（相同值、相同语义）
- **修复**: 移除 `QualityCompressionMode`，统一使用 `QualityMode`
- 同时移除了 `compress_command.cpp` 中的重复 `qualityModeToString` 函数
- 简化了 3 处冗余的同类型 switch 转换代码

#### 消除名称遮蔽
- 匿名命名空间中的 `CompressOptions`、`DecompressOptions`、`InfoOptions`、`VerifyOptions` 与 `commands/` 头文件中的同名类型冲突
- **修复**: 重命名为 `CliCompressOptions`、`CliDecompressOptions`、`CliInfoOptions`、`CliVerifyOptions`

## 影响范围
- `src/main.cpp`
- `src/commands/compress_command.h`
- `src/commands/compress_command.cpp`
