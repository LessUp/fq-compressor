# 去掉 Trellis + OpenSpec 开发模式调整计划

## 背景

项目处于 closeout 模式，变更频率低（每周 < 2 次），AI 为主开发者。
当前使用 Trellis + OpenSpec 双轨制，但对于这种场景流程过于复杂。

**决定**：去掉 Trellis 和 OpenSpec，采用最简化的直接对话开发模式。

---

## 需要删除的内容

### Trellis 相关（约 25+ 文件）

| 路径 | 操作 |
|------|------|
| `.trellis/` | 整个目录删除 |
| `.claude/agents/trellis-*.md` | 删除（3 个文件） |
| `.claude/skills/trellis-*/` | 删除（6 个目录） |
| `.claude/commands/trellis/` | 删除 |
| `.claude/hooks/inject-workflow-state.py` | 删除 |
| `.claude/hooks/inject-subagent-context.py` | 删除 |
| `.claude/hooks/session-start.py` | 删除或重写 |

### OpenSpec 相关（约 15+ 文件）

| 路径 | 操作 |
|------|------|
| `openspec/` | 整个目录删除 |
| `.claude/skills/openspec-*/` | 删除（4 个目录） |
| `.github/prompts/opsx-*.prompt.md` | 删除（4 个文件） |

---

## 需要修改的内容

### 核心文档

| 文件 | 修改内容 |
|------|----------|
| `CLAUDE.md` | 重写为简化的工作流说明 |
| `AGENTS.md` | 移除 Trellis/OpenSpec 引用，更新工作流章节 |
| `README.md` | 移除 OpenSpec 引用 |
| `README.zh-CN.md` | 移除 OpenSpec 引用 |
| `docs/zh/ai-development-mode.md` | 更新为调整后的模式说明 |

### 配置文件

| 文件 | 修改内容 |
|------|----------|
| `.claude/settings.json` | 保持不变或添加权限配置 |

---

## 新的简化工作流

### 文件结构

```
项目根目录/
├── CLAUDE.md          # 主入口文档（简化版）
├── AGENTS.md          # 完整项目说明
├── docs/
│   └── zh/
│       └── ai-development-mode.md  # 开发模式说明（已更新）
└── .claude/
    ├── settings.json  # 基本配置
    └── skills/        # 保留通用 skills（非 trellis）
```

### 工作流（极简版）

```
用户请求 → AI 理解需求 → 直接实现 → 运行测试/lint → 提交
```

### 关键原则

1. **直接对话** - 不需要创建任务、不需要子代理
2. **文档即规范** - AGENTS.md 包含所有编码规范
3. **质量门禁** - 保留 `./scripts/build.sh`、`./scripts/test.sh`、`./scripts/lint.sh`
4. **知识在文档** - 重要决策写入 `docs/` 或代码注释

---

## 实施步骤

### Phase 1: 删除 Trellis

1. 删除 `.trellis/` 目录
2. 删除 `.claude/agents/trellis-*.md`
3. 删除 `.claude/skills/trellis-*/`
4. 删除 `.claude/commands/trellis/`
5. 删除 `.claude/hooks/inject-*.py` 和 `session-start.py`

### Phase 2: 删除 OpenSpec

1. 删除 `openspec/` 目录
2. 删除 `.claude/skills/openspec-*/`
3. 删除 `.github/prompts/opsx-*.md`

### Phase 3: 更新文档

1. 重写 `CLAUDE.md`
2. 更新 `AGENTS.md` 移除 Trellis/OpenSpec 章节
3. 更新 `README.md` 和 `README.zh-CN.md`
4. 更新 `docs/zh/ai-development-mode.md`

### Phase 4: 清理 hooks

1. 删除或简化 `.claude/hooks/` 目录
2. 可能保留一个简单的 session-start.py（如果需要）

---

## 验证

1. 启动新的 Claude Code 会话
2. 确认没有 Trellis/OpenSpec 相关提示
3. 测试基本开发流程：请求 → 实现 → 测试 → 提交

---

## 风险评估

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| 丢失编码规范知识 | 中 | 确保 AGENTS.md 包含所有规范 |
| 丢失历史决策记录 | 低 | docs/ 目录保留文档 |
| Hooks 失效 | 无 | 直接删除即可 |

---

## 关键文件路径

需要修改的核心文件：
- `/home/shane/lessup/fq-compressor/CLAUDE.md`
- `/home/shane/lessup/fq-compressor/AGENTS.md`
- `/home/shane/lessup/fq-compressor/README.md`
- `/home/shane/lessup/fq-compressor/README.zh-CN.md`
- `/home/shane/lessup/fq-compressor/docs/zh/ai-development-mode.md`
