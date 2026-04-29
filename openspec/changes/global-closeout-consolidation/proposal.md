## Why

项目进入 closeout 阶段，前期使用能力较弱的模型开发导致：
- 代码架构漂移（并行/单线程路径语义分歧、完整性校验链路断裂）
- 文档和 GitHub Pages 多源维护、门户价值不足
- CI/CD 过度设计、存在冗余 workflow 和脚本
- AI 工具链配置重复/过时
- 存在未合并的 worktree 和分支

需要一个系统性变更来完成最终收尾，使其达到工业级稳定标准。

## What Changes

- 收口并行/单线程压缩/解压路径的功能漂移
- 修复完整性校验链路（verify、并行解压 verifyChecksums）
- 精简文档和 GitHub Pages 为单一维护面
- 瘦身 CI/CD 为最小高信号配置
- 清理 AI 工具链配置
- 清理 worktree 和分支，确保唯一主线

## Capabilities

### Modified Capabilities

- `compression`: 明确并行路径对 reorder/save-reorder-map/header-only/placeholder-qual/id-prefix 的支持边界
- `decompression`: 确保 verifyChecksums 在并行解压路径生效
- `integrity`: 统一块校验语义，让 verify 真正校验 payload
- `build-system`: 精简 workflow，移除冗余脚本
- `docs-site`: 单一文档源，门户化首页
- `ai-collaboration`: 精简 AI 配置为唯一权威入口
