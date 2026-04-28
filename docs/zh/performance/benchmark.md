---
title: 性能基准测试
description: fq-compressor 的已跟踪基准证据与适用范围
version: 0.2.0
last_updated: 2026-04-28
language: zh
---

# 性能基准测试

> 仓库中的 benchmark 结论现在只基于已跟踪的结果工件。

## 已跟踪证据

- JSON 工件：`benchmark/results/err091571-local-supported.json`
- Markdown 工件：`benchmark/results/err091571-local-supported.md`
- 数据集清单：`benchmark/datasets.yaml`
- 运行入口：`scripts/benchmark.sh`

## 当前已验证范围

当前已跟踪结果集是有意收窄的。

- 数据集：ENA `ERR091571`
- 工作负载：公开 Illumina 双端人类 WGS 运行
- 实测输入：从 read 1 公共 FASTQ 流式截取的前 2,000 条记录，输出到 `benchmark/data/ERR091571/ERR091571_1.2000.fastq`
- 已测本地工具：`fqc`、`gzip`、`xz`、`bzip2`
- 已显式延后专用对手：`spring`

这意味着当前仓库只能对这个工作负载切片上的本地支持工具集给出结论。

## 当前调研结论

根据 `benchmark/results/err091571-local-supported.json`：

- 压缩比：`fq-compressor` 不是当前已测集合中的最优，`bzip2` 更好
- 压缩速度：`fq-compressor` 不是当前已测集合中的最优，`bzip2` 更快
- 解压速度：`fq-compressor` 不是当前已测集合中的最优，`xz` 更快
- FASTQ 专用同类排名：仍未证实，因为 `spring` 被明确标记为延后而非已测

所以当前能被证据支持的结论不是“`fq-compressor` 已经处于第一梯队”，而是：在当前本地支持且已测量的维度上，它并未领先；而对专用同类工具的比较仍然不完整。

## 为什么当前跟踪数据集较小

benchmark 框架现在通过公共数据清单记录 provenance，并把来源信息写入结果工件。

当前正式跟踪工件固定为 2,000 条记录，原因是：

- 它直接来自公开 ENA 数据源
- 它能在 closeout 验证回路内稳定完成
- 当前 `fq-compressor` 构建在同一工作负载的 20,000-record 探索子集上出现明显性能回归

更大的子集仍适合后续排查，但目前不作为正式跟踪证据。

## 重现方式

```bash
./scripts/benchmark.sh \
  --dataset err091571-local-supported \
  --prepare \
  --build \
  --tools fqc,gzip,xz,bzip2,spring \
  --threads 1 \
  --runs 1
```

这会刷新：

- `benchmark/results/err091571-local-supported.json`
- `benchmark/results/err091571-local-supported.md`

## 范围边界

不要把当前结果理解成普遍排名。

- 它只覆盖一个公开工作负载切片
- 它只覆盖当前本地支持工具集
- 它明确把延后专用对手排除在“已验证比较范围”之外

## 相关说明

- benchmark 框架说明见 `docs/benchmark/README.md`
- 完整跟踪报告见 `benchmark/results/err091571-local-supported.md`

---

**🌐 语言**: [English](../../en/performance/benchmark.md) | [简体中文（当前）](./benchmark.md)
