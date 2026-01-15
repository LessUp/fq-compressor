# Spring 读长兼容性与格式一致性修复（2026-01-15）

## 概述

针对 Spring README 中提到的 511bp 读长限制，补充读长分类与回退策略，同时修正文件布局中 Reorder Map 的位置描述，确保格式与实现策略一致。

## 修改文件

- `.kiro/specs/fq-compressor/design.md`
- `.kiro/specs/fq-compressor/requirements.md`
- `.kiro/specs/fq-compressor/tasks.md`

## 变更内容

1. **文件格式布局对齐**
   - 明确 Reorder Map 位于 Block Index 之前，便于随机访问与前向读取。
2. **读长分类与 Spring 兼容性补充**
   - Short 模式增加 `max_read_length <= 511` 约束；否则切换到 Medium/Long 或通用压缩。
   - 自动检测流程加入 `max_length` 判定，避免误入 ABC 限制路径。
3. **任务计划同步**
   - 长读检测任务补充 `max_length` 计算与 511bp 保护。
   - 长读压缩策略增加 Spring 兼容的 fallback 说明。

## 影响

- 读长判定与压缩策略对齐 Spring 能力边界，减少运行期失败风险。
- 文件格式说明与 Reorder Map 逻辑保持一致，降低实现歧义。
