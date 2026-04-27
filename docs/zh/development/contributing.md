---
title: 贡献指南
description: fq-compressor 的 closeout 阶段贡献工作流
version: 0.2.0
last_updated: 2026-04-27
language: zh
---

# 贡献指南

`fq-compressor` 当前处于 **closeout mode**。贡献应优先聚焦于**收尾、精简、稳定、修复和文档治理**，而不是继续扩展产品表面。

## 仓库姿态

- **OpenSpec 驱动**：`openspec/specs/` 是唯一活规范源
- **一次只做一个 active change**：避免再开平行清理战线
- **文档分层明确**：尽量保留一个权威解释，而不是多个镜像版本
- **工具链最小化**：当更简单的流程已经足够时，不再引入额外维护面

## 事实来源边界

请有意识地区分这些表面：

| 表面 | 角色 |
| --- | --- |
| `openspec/specs/` | 活规范 |
| `openspec/changes/` | 当前变更与执行上下文 |
| `README.md` / `README.zh-CN.md` | 仓库入口摘要 |
| `docs/website/` | GitHub Pages 源码 |
| `docs/en/` / `docs/zh/` | 维护中的参考文档 |
| `specs/` | 仅历史参考，不是当前规范 |
| `docs/archive/` | 仅历史参考，不是当前事实 |

如果你准备修改 `specs/` 或 `docs/archive/`，请先停下来确认：这份内容是否真的应该继续留在归档表面，而不是回到权威位置。

## 默认收尾工作流

1. 先运行仓库预检：
   ```bash
   ./scripts/dev/preflight.sh
   openspec list --json
   ```
2. 直接对齐当前 active OpenSpec change，不要依据自由发挥的提示词单独施工。
3. 先做最小必要修改和定向检查，再跑默认门禁。
4. 保持默认本地门禁为绿色：
   ```bash
   ./scripts/lint.sh format-check
   ./scripts/test.sh clang-debug
   ```
5. 当 diff 风险较高、跨文件较多或带有架构性质时，使用 `/review`。
6. 相关检查通过后直接推送即可；对本仓库而言，PR 是可选项，不是默认单人维护路径。

## 可选隔离方式

默认工作流允许直接在当前分支上推进。如果你需要额外隔离，可以根据 OpenSpec change 名称创建 worktree：

```bash
./scripts/dev/create-worktree.sh <change-name>
```

这是**可选项**，不是强制流程。

## 构建、测试与静态检查

尽量使用仓库现有脚本，而不是临时拼接命令：

```bash
./scripts/build.sh clang-debug
./scripts/test.sh clang-debug
./scripts/lint.sh format-check
./scripts/lint.sh lint clang-debug
```

C++ 编辑器/LSP 的主路径应保持为：

- `clangd`
- `compile_commands.json`
- CMake presets
- 根目录 `.clangd`

除非有明确、成文的理由，否则不要再引入第二套项目级 C++ 语言服务主栈。

## 当前阶段适合的贡献类型

适合当前收尾阶段的贡献：

- 删除重复或过时文档
- 收紧 README / Pages / docs 的职责边界
- 重新验证并修复真实缺陷，同时补充聚焦回归测试
- 精简 CI、Release、Pages 工作流
- 收敛 AI 指南与本地工具配置

当前阶段不鼓励或应提高门槛的贡献：

- 增加大型新功能
- 在没有充分证据的情况下扩张 CI 矩阵
- 引入新的大型工具链或治理层
- 仅仅为了“以防万一”保留重复文档

## 文档归属原则

只更新真正拥有该信息的表面：

- 仓库入口摘要 -> `README*.md`
- 对外 landing story -> `docs/website/`
- 维护中的参考文档 -> `docs/en/` / `docs/zh/`
- 需求与工作流规则 -> `openspec/`

优先考虑**删除、归档、合并**，而不是把同一段说明复制到更多文件。

## 审查与收尾

完成一个非平凡变更前，请确认：

1. 已重跑相关检查；
2. 变更仍然对齐当前 active OpenSpec tasks；
3. 如果 diff 风险较高，已使用 `/review`；
4. 仓库状态仍足够清晰，方便后续低上下文维护者或模型接手。

## 许可证

通过贡献，您同意您的贡献将按照 MIT 许可证授权。
