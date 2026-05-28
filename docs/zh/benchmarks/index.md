# 性能证据

这一节是整个站点的证据契约。它的职责不是把 fq-compressor 讲得无限强，而是把仓库今天能证明什么和仍属于设计意图的部分清清楚楚分开。

<EvidenceGrid locale="zh" />

## 当前公开边界

- 当前被跟踪的公开证据，主要围绕 ENA accession `ERR091571` 与可复现的 2,000-record smoke-scale 子集。
- 站点可以讨论压缩密度、压缩吞吐、解压吞吐与归档语义，但不能把自己描述成已经被证明领先所有 FASTQ 专用同类。
- 由于 Spring 仍处于 deferred 状态，当前 benchmark 叙事应被理解为有边界的，而不是完整结论。
- 新的已跟踪 benchmark 证据应通过 `./scripts/benchmark.sh` 生成；`./scripts/benchmark_v2.sh` 则保留为本地 exploratory / comparison CLI。
- 当前压缩比和吞吐的区间位置，应以上述生成报告为准，而不是继续引用仓库里的写死文案。

## 主张矩阵

| 主张 | 当前公开强度 | 原因 |
| --- | --- | --- |
| fq-compressor 有一组被记录的短读长压缩结果 | 强 | `benchmark/results/` 中有跟踪产物 |
| fq-compressor 已经是同类最佳 | 不能公开宣称 | Spring 仍未纳入完成的对照证明链 |
| O(1) 随机访问是系统契约的一部分 | 强 | 有格式设计、架构文档与代码锚点共同支撑 |
| 吞吐必须和归档语义一起解读 | 强 | 站点把写路径、读路径和检索代价写成同一条故事线 |

## 建议同时打开的证据锚点

- [方法学](./methodology)
- `./scripts/benchmark.sh --dataset err091571-local-supported --build --threads 1 --runs 1`
- `./scripts/benchmark_v2.sh run --json <path> --report <path>`
- [`docs/benchmark/README.md`](https://github.com/LessUp/fq-compressor/blob/master/docs/benchmark/README.md)
- [`benchmark/results/err091571-local-supported.json`](https://github.com/LessUp/fq-compressor/blob/master/benchmark/results/err091571-local-supported.json)
- [`benchmark/results/err091571-local-supported.md`](https://github.com/LessUp/fq-compressor/blob/master/benchmark/results/err091571-local-supported.md)

## 下一步

- [算法白皮书](/zh/whitepaper/)：看这些指标背后的技术命题
- [参考研究](/zh/research/)：看论文、对照项目与项目姿态
