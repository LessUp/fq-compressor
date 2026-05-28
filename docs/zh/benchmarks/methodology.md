---
title: 基准测试方法
description: 已跟踪结果的范围、证据规则与复现命令
---

# 基准测试方法

基准测试部分是证据索引，不是营销排行榜。
它的职责是说明：哪些公开表述已经被仓库中的跟踪工件支撑，以及当前比较边界到哪里为止。

## 证据规则

当前 benchmark 工作流遵循 `docs/benchmark/README.md` 中已经写明的规则：

- 每个公开结论都应指向仓库内已跟踪的结果工件
- 数据集来源必须公开且可复现
- requested、measured、unavailable、deferred 的工具都要显式列出
- 结论应按维度分别陈述，而不是给出单一总体排名

这些规则现在统一由 `benchmark_v2/` 这套执行 / 报告栈落地；`./scripts/benchmark.sh` 只是面向已跟踪证据链的
wrapper，而公开 ERR091571 证据链的数据集注册表仍保留在 `benchmark/datasets.yaml`。

## 当前跟踪的数据集与工件

仓库当前公开了一条已跟踪证据链：

- dataset id：`err091571-local-supported`
- 公开来源：ENA accession `ERR091571`
 - 跟踪输入：基于公开 ENA accession `ERR091571` 复现准备得到的 2,000 条记录子集
- 机器可读结果：`benchmark/results/err091571-local-supported.json`
- 人类可读报告：`benchmark/results/err091571-local-supported.md`

跟踪工作负载故意保持较小：它是从公开 read 1 FASTQ 复现准备得到的 2,000 条记录子集。
这样 closeout 验证回路仍然可复现，同时也不会暗示仓库把 benchmark 输入本身检入了版本库，更不会假装已经证明了更大范围的生产级主张。

## 工具范围

当前跟踪运行请求 `fqc`、`gzip`、`xz`、`bzip2` 和 `spring`。
其中当前真正进入 measured local-supported 集合的是：

- `fq-compressor`
- `gzip`
- `xz`
- `bzip2`

`Spring` 仍在已跟踪工件中被明确标记为 deferred，因此仓库目前还没有足够证据支撑“与 FASTQ 专用同类工具正面对比”的公开结论。

## 复现路径

文档中的复现链路是：

```bash
./scripts/benchmark.sh --dataset err091571-local-supported --prepare-only
./scripts/build.sh clang-release
./scripts/benchmark.sh \
  --dataset err091571-local-supported \
  --build \
  --tools fqc,gzip,xz,bzip2,spring \
  --threads 1 \
  --runs 1
```

这条命令链与 `docs/benchmark/README.md` 以及生成出的报告本身保持一致。底层现在统一走
`benchmark_v2/`，并产出 canonical `benchmark_report_v2` JSON / Markdown 工件。

## 应如何解读当前数字

当前跟踪报告只支持有边界的结论。
在这个小型公共子集和 local-supported 工具集上：

- `fq-compressor` 的压缩比不是最优
- `fq-compressor` 的压缩速度不是最优
- `fq-compressor` 的解压速度不是最优

这些表述的价值恰恰来自边界清晰。
它们描述的是仓库中已经测得并保存下来的工件，而不是对所有 FASTQ 工具、所有数据集、所有运行点做无条件排名。

## 哪些内容不算公开证据

本地 smoke test 或便捷运行仍然可以用于验证工具链是否正常，但如果数据集、命令路径和输出没有被仓库跟踪，它们就不应被视为公开 benchmark 证据。
也正因此，这里的页面始终回指已检入的 JSON/Markdown 工件，而不是未跟踪截图或私有数据路径。
