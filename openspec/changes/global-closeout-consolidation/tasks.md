## 1. 代码架构收口

- [ ] 1.1 统一块校验语义：明确校验对象是逻辑未压缩数据，更新 BlockCompressor/CompressorNode/FQCReader/VerifyCommand
- [ ] 1.2 让 verify 命令真正校验 block payload，而不只是 header/index
- [ ] 1.3 把 verifyChecksums 接入并行解压路径（FQCReaderNode）
- [ ] 1.4 增加 archive 篡改后 verify/decompress 应失败的回归测试
- [ ] 1.5 明确处理 threads=1 vs threads>1 的行为差异（--header-only, --placeholder-qual, --id-prefix, --reorder, --save-reorder-map）
- [ ] 1.6 增加"线程数不改变 CLI 语义"的测试矩阵

## 2. 文档与 GitHub Pages 门户化

- [ ] 2.1 决定唯一参考源（保留 docs/website/pages/**，压缩 docs/en|zh/**）
- [ ] 2.2 删除/隔离本地构建产物（docs/.site, docs/website/.next, docs/website/node_modules）
- [ ] 2.3 重写 GitHub Pages 首页为门户页（定位/场景/证据/CTA）
- [ ] 2.4 清理 FAQ 中无 artifact 支撑的性能话术
- [ ] 2.5 修复中英文站点导航不对称
- [ ] 2.6 更新 docs/README.md 的 ownership map

## 3. CI/CD 精简

- [x] 3.1 合并 quality.yml 到 ci.yml
- [x] 3.2 精简 ci.yml 为 format-check + clang-debug build/test + gcc-release build
- [ ] 3.3 精简 pages.yml 直接使用 docs/website npm 命令
- [ ] 3.4 瘦身 release.yml（修正 tag 触发，减少 matrix）
- [ ] 3.5 删除失效 benchmark/report/chart 脚本
- [ ] 3.6 清理根目录旧 Node 文档依赖

## 4. AI 工具链治理

- [x] 4.1 瘦身 AGENTS.md 为唯一权威规则（保留完整内容作为参考）
- [x] 4.2 让 CLAUDE.md 和 copilot-instructions.md 变成短入口指向 AGENTS.md
- [x] 4.3 删除 .claude/index.json, .claude/settings.local.json
- [ ] 4.4 合并/删除 .claude/commands 和 .claude/skills 重复层
- [x] 4.5 删除 QWEN.md 等过时文档

## 5. 分支与 Worktree 净化

- [x] 5.1 收尾 benchmark-hardening worktree（合并或删除分支）
- [x] 5.2 确保只保留主线分支
- [x] 5.3 使用 gh 更新 repo description/topics/Pages URL

## 6. 最终验证

- [x] 6.1 运行 ./scripts/dev/preflight.sh
- [x] 6.2 运行 openspec list --json 确认无活跃变更（当前有 global-closeout-consolidation）
- [x] 6.3 运行 ./scripts/lint.sh format-check
- [x] 6.4 运行 ./scripts/test.sh clang-debug
- [ ] 6.5 运行 docs website build
- [x] 6.6 提交并推送
