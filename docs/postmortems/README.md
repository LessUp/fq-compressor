# 复盘记录

本目录收录 fq-compressor 开发中遇到的非平凡问题复盘，聚焦根因而非结果。
CHANGELOG 只记"做了什么"，这里记"为什么发生、如何定位、如何避免再犯"。

## 约定

- 文件名：`YYYY-MM-DD-slug.md`，日期用问题**定位当日**。
- 固定六节：症状 / 复现 / 根因 / 修复 / 验证 / 教训。
- 一篇只写一个问题；复杂事故可拆多篇互相引用。
- 修复合入后在对应 CHANGELOG "修复"条目末尾加 `→ 详见 docs/postmortems/...`。

## 索引

| 日期 | 问题 | 文件 |
|---|---|---|
| 2026-07-24 | SPSC `pop()` 在 `close` 后丢失尾帧 | [2026-07-24-spsc-pop-lost-tail-frame.md](2026-07-24-spsc-pop-lost-tail-frame.md) |
