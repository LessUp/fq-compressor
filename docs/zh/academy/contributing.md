# 贡献

`fq-compressor` 目前处于 **closeout mode**。相比扩展产品面，更欢迎聚焦于收尾、简化、文档收紧，以及有证据支撑的缺陷修复。

## 从当前主线开始

```bash
git clone https://github.com/LessUp/fq-compressor.git
cd fq-compressor
git checkout master
```

仓库默认工作流是直接推送主线。如果改动较大，需要隔离时再从 `master` 拉出一个短生命周期分支即可。

## 推荐的本地工作流

```bash
./scripts/build.sh clang-debug
./scripts/test.sh clang-debug
./scripts/lint.sh format-check
```

如果你的变更涉及 C++ 代码路径，再补跑一次 `./scripts/lint.sh lint clang-debug` 做静态分析确认。

## 让改动保持小而清晰

优先处理符合当前项目姿态的工作：

- 收紧命令行为或诊断输出
- 改进基准证据链和复现路径
- 简化工具链与文档
- 修复正确性问题，并补上回归覆盖

不要在仓库证据没有同步变化时扩大公开主张。

## 文档更新要求

根据你修改的对象更新对应文档：

- GitHub Pages 内容：`docs/en/` 或 `docs/zh/`
- 仓库侧参考材料：`README.md`、`AGENTS.md`、`docs/benchmark/README.md`
- 指向仓库文件的链接应使用 `master` 分支

## 交付前检查

```bash
git --no-pager diff --stat
./scripts/test.sh clang-debug
./scripts/lint.sh format-check
```

如果只是文档改动，`cd docs && npm run build` 就是对公开站点最相关的检查。

## 继续阅读

- [快速开始](./getting-started)：重新搭起本地环境
- [CLI 工作流](./cli-workflows)：查看问题复现和评审常用命令
- [研究](/zh/research/)：查看规格、基准产物与参考资料
