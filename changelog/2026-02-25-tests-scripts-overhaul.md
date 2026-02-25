# Tests & Scripts 全面优化

**日期**: 2026-02-25

## 关键修复

### tests/CMakeLists.txt — 缺失测试注册（严重）
- **新增** `fastq_parser_property_test`（io/fastq_parser_property_test.cpp）注册
- **新增** `pipeline_property_test`（pipeline/pipeline_property_test.cpp）注册
- 这两个测试文件之前存在于磁盘但从未被编译或执行
- **移除** 冗余的 `enable_testing()` 调用（根 CMakeLists.txt 已通过 `include(CTest)` 调用）
- **移除** 死代码：placeholder_test.cpp 的内联生成逻辑（文件已存在，条件永远为 false）
- **修复** Property test 标签被覆盖的问题，改为条件分支赋值

### scripts/run_tests.sh — 构建目录映射错误（严重）
- **重写** 为基于 CMake preset 系统，不再使用 compiler+type 二维映射
- **修复** GCC Debug 错误映射到 `build/Debug`（应为 `build/gcc-debug`）
- **修复** Clang Debug 完全缺失映射
- **修复** 管道状态 bug（`$?` 检测的是 `tail` 而非构建命令，改用 `PIPESTATUS`）
- **移除** 硬编码测试目标名，改用 `cmake --build --preset` + `ctest`
- **新增** `--label` 选项支持按标签运行（unit/property）
- **新增** `--build-only` 选项

## 一般优化

### .gitignore
- 添加 `/Testing/` 目录，防止 CTest 工件泄入版本库

### Testing/Temporary/CTestCostData.txt
- 从版本库中移除（git rm --cached）

### tests/.gitkeep → tests/README.md
- 将含内容的 `.gitkeep` 转换为正式的 `README.md` 文档
- 补充目录结构说明和运行测试的示例命令

### tests/placeholder_test.cpp
- C++ 标准检查从 C++20 更新为 C++23（兼容 C++20 回退）

### Shell 脚本统一
- **shebang 统一**: 所有脚本改为 `#!/usr/bin/env bash`
- **build.sh**: 添加 `nproc` 跨平台回退
- **install_deps.sh**: 修复 `$COMPILER_SETTINGS` 未引用导致的 word-splitting 风险
- **lint.sh**: 改用 `mapfile` + 数组传参，消除 `echo | xargs` 的文件名空格问题；添加 build 路径排除
- **benchmark.sh**: 修复 `show_help` 在未知选项时退出码为 0 的问题；移除 `cd` 改用绝对路径调用
- **compare_spring.sh**: 修复 FQC 二进制搜索路径（从错误的 `build/build/Release/bin/` 改为 `build/<preset>/src/`）；修复颜色代码嵌套问题
- **test.sh**: shebang 统一

### Python 脚本
- **generate_benchmark_charts.py**: 添加输入文件存在性检查、JSON 解析错误处理、必需键检查、空结果处理
- **generate_benchmark_report.py**: 同上
