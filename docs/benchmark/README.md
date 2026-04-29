# fq-compressor Benchmark Framework

本文档说明当前仓库中的基准测试证据链、数据来源，以及如何重新生成已跟踪的结果文件。

## 当前进展

- 当前已跟踪结果文件：`benchmark/results/err091571-local-supported.json`
- 对应报告：`benchmark/results/err091571-local-supported.md`
- 当前公开数据集：ENA `ERR091571`
- 当前已验证工具范围：`fqc`、`gzip`、`xz`、`bzip2`
- 当前显式延后工具：`spring`

## 为什么要改成跟踪结果

旧版 benchmark 文档混合了历史数字、未跟踪结果表和仓库内不存在的私有数据路径。现在的规则是：

- 公共 benchmark 结论必须指向仓库内的 JSON/Markdown 结果文件
- 数据集来源必须可公开重现
- 已请求、已测量、缺失和延后的工具必须显式记录
- 结论必须按维度拆开，而不是给出一个无边界的“第一梯队”总排名

## 数据集清单

数据集清单位于 `benchmark/datasets.yaml`。

当前跟踪条目：`err091571-local-supported`

- 来源：ENA `ERR091571`
- 工作负载：Illumina 双端人类 WGS 运行的一部分
- 当前跟踪输入：`benchmark/data/ERR091571/ERR091571_1.2000.fastq`
- 说明：为了保持 closeout 验证回路可执行，当前跟踪工件固定为从公共 `ERR091571` read 1 流式截取的前 2,000 条记录
- 限制：更大的 20,000-record 子集也可流式准备，但当前 `fq-compressor` 构建在该规模上出现明显性能回归，因此未作为正式跟踪工件

## 当前调研结论

基于 `benchmark/results/err091571-local-supported.json` 的当前可证实结论：

- 压缩比：在本次已测量集合里，`fq-compressor` 不是最优；`bzip2` 更好
- 压缩速度：在本次已测量集合里，`fq-compressor` 不是最优；`bzip2` 更快
- 解压速度：在本次已测量集合里，`fq-compressor` 不是最优；`gzip` 更快
- FASTQ 专用同类对比：`Spring` 仍处于延后状态，因此当前仓库还不能回答 `fq-compressor` 相对专用同类工具是否处于第一梯队

这意味着当前仓库可以诚实地说：

- 对这个已测量的小型公共子集和当前本地支持工具集，`fq-compressor` 尚未在压缩比、压缩速度或解压速度上领先
- 对 FASTQ 专用对手的比较仍未证实，不能用这份结果做普遍排名宣传

## 如何重跑

1. 准备公共数据子集

```bash
./scripts/benchmark.sh --dataset err091571-local-supported --prepare-only
```

2. 构建 release 二进制

```bash
./scripts/build.sh clang-release
```

3. 生成跟踪结果

```bash
./scripts/benchmark.sh \
  --dataset err091571-local-supported \
  --build \
  --tools fqc,gzip,xz,bzip2,spring \
  --threads 1 \
  --runs 1
```

4. 直接使用 Python 运行器也可以

```bash
python3 benchmark/benchmark.py \
  --dataset err091571-local-supported \
  --tools fqc,gzip,xz,bzip2,spring \
  --threads 1 \
  --runs 1 \
  --json benchmark/results/err091571-local-supported.json \
  --report benchmark/results/err091571-local-supported.md
```

## 结果结构

`benchmark/results/err091571-local-supported.json` 现在包含：

- 数据集 provenance
- 基准命令与工作目录
- 请求工具列表
- 已测量工具列表
- 不可用工具列表
- 延后工具列表
- 原始结果条目
- 按维度拆开的结论摘要

## Smoke Test

仓库内基准链路的功能验证另外使用你本机的小样本数据完成：

- `/home/shane/data/test/small/test_1.fq.gz`
- `/home/shane/data/test/small/test_2.fq.gz`

这些小样本用于验证脚本与工具链工作正常，不作为公开 benchmark 证据工件。
