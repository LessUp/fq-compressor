# brainstorm: 项目彻底重构与归档

## Goal

对 fq-compressor 项目进行全方位彻底重构与规范化，修复所有已知 Bug，精简工程化与 AI 工具链，达到工业级稳定、可归档的最终完结状态。

## What I already know

### 项目基本信息
- **项目名称**: fq-compressor (C++23 FASTQ 压缩工具)
- **版本**: 0.2.0
- **状态**: closeout mode (收尾模式)
- **架构**: ABC + SCM 压缩算法，TBB 并行管道，O(1) 随机访问

### 当前项目状态

#### 已完成的任务（来自 global-closeout-consolidation/tasks.md）
1. 代码架构收口: 1.4 ✅
2. 文档与 GitHub Pages: 2.1 ✅, 2.2 ✅, 2.4 ✅, 2.6 ✅
3. CI/CD 精简: 3.1-3.4 ✅
4. AI 工具链治理: 4.1-4.5 ✅
5. 分支与 Worktree 净化: 5.1-5.3 ✅
6. 最终验证: 6.1-6.4 ✅, 6.6 ✅

#### 待完成的任务
1. 代码架构收口:
   - 1.1 统一块校验语义
   - 1.2 让 verify 命令真正校验 block payload
   - 1.3 把 verifyChecksums 接入并行解压路径
   - 1.5 明确 threads=1 vs threads>1 的行为差异
   - 1.6 增加"线程数不改变 CLI 语义"的测试矩阵

2. 文档与 GitHub Pages:
   - 2.3 重写 GitHub Pages 首页为门户页
   - 2.5 修复中英文站点导航不对称

3. CI/CD 精简:
   - 3.5 删除失效 benchmark/report/chart 脚本
   - 3.6 清理根目录旧 Node 文档依赖

4. 最终验证:
   - 6.5 运行 docs website build

### 发现的问题

#### 架构问题
1. 存在 34 个 AI 工具配置目录（.cursor, .windsurf, .cline, .qwen 等），大部分冗余
2. docs/archive/ 目录包含大量过时文档
3. specs/ 目录标记为 archive-only，但仍然存在
4. 多个 .md 文档重复或过时

#### 测试状态
- 构建通过 (clang-debug)
- 格式检查通过
- 8/9 测试通过 (单独运行 async_io_test 通过，并发运行时偶发失败)

#### 分支状态
- 仅存在 master 分支 (干净)
- 无活跃 worktree

## Assumptions (temporary)

1. 用户希望激进清理，而非保守保留
2. 最终状态需要"可随时归档"，即无需对外声明停止维护
3. 中英文双语文档都需要保留和完善

## Open Questions

1. **隐藏目录清理策略**: 34 个 AI 工具目录中，除了 `.claude` 和 `.trellis`，其他是否全部删除？
   - 建议：仅保留 `.claude`, `.trellis`, `.git`, `.github`, `.devcontainer`, `.vscode`
   - 删除其他 28+ 个目录

2. **docs/archive 处理**: 是否保留 archive 目录作为历史参考？
   - 建议：保留但精简，确保 README.md 清晰声明其 archive 状态

3. **specs/ 目录**: 标记为 archive-only，是否移动到 docs/archive/ 下？
   - 建议：是，统一归档到 docs/archive/specs/

## Requirements (evolving)

### Phase 1: 架构与代码库清洗

1. **隐藏目录清理**
   - 保留：.claude, .trellis, .git, .github, .devcontainer, .vscode
   - 删除：其他 28+ 个 AI 工具配置目录

2. **文档治理**
   - 将 specs/ 移动到 docs/archive/specs/
   - 清理 docs/archive/ 中过时文档
   - 确保 docs/README.md 准确反映当前文档结构

3. **GitHub Pages 门户化**
   - 重写首页 (index.mdx) 为高转化门户页
   - 修复中英文导航不对称
   - 确保文档构建通过

4. **测试修复**
   - 确保所有测试在并发环境下稳定通过
   - 添加缺失的测试矩阵

### Phase 2: 工程化精简

1. **清理失效脚本**
   - 删除 benchmark/report/chart 相关失效脚本

2. **清理 Node 依赖**
   - 清理根目录旧 Node 文档依赖 (package.json, package-lock.json)

3. **更新 GitHub 元数据**
   - 使用 gh CLI 更新 description, topics, Pages URL

### Phase 3: AI 工具链优化

1. **AGENTS.md 完善**
   - 确保作为唯一权威规则文档
   - 深度注入项目业务逻辑

2. **CLAUDE.md 完善**
   - 作为 AGENTS.md 的短入口

3. **copilot-instructions.md 完善**
   - 作为 AGENTS.md 的短入口

### Phase 4: 最终验证

1. 运行完整测试套件
2. 运行文档构建
3. 提交并推送
4. 验证 CI 通过

## Acceptance Criteria (evolving)

- [ ] 所有测试通过 (并发环境稳定)
- [ ] 文档构建成功
- [ ] GitHub Pages 首页门户化完成
- [ ] 中英文导航对称
- [ ] 隐藏目录从 34 个减少到 6 个
- [ ] docs/archive/ 结构清晰
- [ ] specs/ 移动到 docs/archive/specs/
- [ ] AGENTS.md 作为唯一权威入口
- [ ] CI 通过
- [ ] 代码库干净（无冗余文件）

## Definition of Done (team quality bar)

- 所有测试通过
- Lint/格式检查通过
- 文档构建成功
- CI 全绿
- 本地与远程代码库干净

## Out of Scope (explicit)

- 不添加新功能
- 不修改核心压缩/解压算法
- 不创建新分支或 worktree
- 不进行大规模代码重构

## Technical Notes

### 关键文件路径
- 任务目录: `.trellis/tasks/05-06-project-refactor-archive/`
- OpenSpec 变更: `openspec/changes/global-closeout-consolidation/`
- 文档站点: `docs/website/`
- 核心规范: `AGENTS.md`, `CLAUDE.md`

### 构建命令
- `./scripts/build.sh clang-debug`
- `./scripts/test.sh clang-debug`
- `./scripts/lint.sh format-check`
- `npm run docs:build` (在 docs/website/)

### 隐藏目录清单 (待删除)
```
.codex, .cospec, .roo, .gemini, .clinerules, .lingma, .forge,
.windsurf, .omc, .bob, .amazonq, .crush, .kiro, .opencode,
.qoder, .kilocode, .cursor, .agent, .codebuddy, .iflow,
.continue, .qwen, .pi, .factory, .augment, .junie, .trae, .cline
```
