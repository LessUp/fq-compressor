# CMake 预设工具链路径修复

日期：2026-01-29

## 问题描述

执行 `cmake --preset gcc-release && cmake --build build/gcc-release` 时出现以下错误：

1. 找不到 Conan 工具链文件：
   ```
   CMake Error: Could not find toolchain file:
   /workspace/build/gcc-release/build/Debug/generators/conan_toolchain.cmake
   ```

2. 找不到 Ninja 构建工具（实际上 Ninja 存在，但由于工具链配置错误导致 CMake 无法正确初始化）

## 根本原因

`CMakePresets.json` 中的基础预设 `conan-gcc-base` 和 `conan-clang-base` 硬编码了工具链路径指向 `Debug` 目录：

```json
"toolchainFile": "${sourceDir}/build/${presetName}/build/Debug/generators/conan_toolchain.cmake"
```

但 Conan 为不同的构建类型（Debug/Release）会在对应的目录下生成工具链文件：
- Debug 构建：`build/${presetName}/build/Debug/generators/`
- Release 构建：`build/${presetName}/build/Release/generators/`

## 解决方案

将 `conan-gcc-base` 和 `conan-clang-base` 拆分为 Debug 和 Release 版本：

- `conan-gcc-debug-base` → 指向 Debug 目录
- `conan-gcc-release-base` → 指向 Release 目录
- `conan-clang-debug-base` → 指向 Debug 目录
- `conan-clang-release-base` → 指向 Release 目录

更新了各预设的继承关系：
| 预设 | 原继承 | 新继承 |
|------|--------|--------|
| gcc-debug | conan-gcc-base | conan-gcc-debug-base |
| gcc-release | conan-gcc-base | conan-gcc-release-base |
| gcc-relwithdebinfo | conan-gcc-base | conan-gcc-release-base |
| clang-debug | conan-clang-base | conan-clang-debug-base |
| clang-release | conan-clang-base | conan-clang-release-base |
| clang-asan | conan-clang-base | conan-clang-debug-base |
| clang-tsan | conan-clang-base | conan-clang-debug-base |

## 修改的文件

- `CMakePresets.json`：拆分基础预设并更新继承关系

## 验证

1. 清理旧的构建目录：`rm -rf build/gcc-release`
2. 安装 Conan 依赖：`conan install . --build=missing --output-folder=build/gcc-release -s build_type=Release`
3. 配置并构建：`cmake --preset gcc-release && cmake --build build/gcc-release -j$(nproc)`
4. 验证二进制文件：`./build/gcc-release/src/fqc --version` → 输出 `0.1.0`

## 正确的构建流程

```bash
# 1. 安装依赖（首次或依赖变更后）
conan install . --build=missing --output-folder=build/gcc-release -s build_type=Release

# 2. 配置
cmake --preset gcc-release

# 3. 构建
cmake --build build/gcc-release -j$(nproc)
```
