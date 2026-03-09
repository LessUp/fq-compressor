# GitHub Pages 优化 (2026-03-10)

## CI/CD 工作流

1. **FIXED**: `pages.yml` — 修正 paths 触发器中错误的自引用路径（`docs-pages.yml` → `pages.yml`）
2. **OPTIMIZED**: `pages.yml` — paths 触发范围从 `docs/**` + `README.md` 收窄为 `docs/gitbook/**`，避免无关文件变更触发构建
3. **OPTIMIZED**: `pages.yml` — 添加 sparse-checkout，仅拉取 `docs/gitbook`、`package.json`、`package-lock.json`，跳过 src/tests/benchmark/vendor 等大目录
4. **SYNCED**: `docs-quality.yml` — paths 触发范围同步收窄为 `docs/gitbook/**`

## Honkit 文档站

5. **FIXED**: `en/README.md` — 修复断链 `architecture.md` → `design.md`
6. **FIXED**: `en/SUMMARY.md` + `zh/SUMMARY.md` — 将孤立的 `usage.md` 加入目录导航
7. **FIXED**: `en/installation.md` + `zh/installation.md` — 编译器版本要求与 README 同步（GCC 15+/Clang 21+ → GCC 13+/Clang 16+）
8. **IMPROVED**: `book.json` — 语言设为 `en,zh` 适配多语言站点；添加 Rust Version 侧边栏链接；替换默认搜索为 search-pro 以支持中文搜索
9. **ADDED**: `package.json` — 添加 `gitbook-plugin-search-pro` 依赖
