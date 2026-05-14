# 基准测试

基准测试部分的存在目的，是让所有公开性能表述都能回到可复现的证据链。
它应回答两个问题：仓库里到底测过什么，以及当前证明边界之外还剩下什么。

## 从这里开始

- [基准测试方法](/zh/benchmarks/methodology) 说明证据规则、数据集范围与复现命令。
- [`docs/benchmark/README.md`](https://github.com/LessUp/fq-compressor/blob/master/docs/benchmark/README.md) 是仓库侧对当前跟踪 benchmark 链路的说明。
- [`benchmark/results/err091571-local-supported.json`](https://github.com/LessUp/fq-compressor/blob/master/benchmark/results/err091571-local-supported.json) 是机器可读的跟踪结果。
- [`benchmark/results/err091571-local-supported.md`](https://github.com/LessUp/fq-compressor/blob/master/benchmark/results/err091571-local-supported.md) 是面向阅读的跟踪报告。

## 当前证据

仓库目前跟踪一组公开证据，数据来自 ENA accession `ERR091571`，使用一个可复现的 2,000-record 烟雾规模子集。
在这次已文档化的运行中，measured local-supported 工具范围内观察到的领先者是：

- 最优压缩比：`bzip2`
- 最优压缩速度：`bzip2`
- 最优解压速度：`gzip`
- `fq-compressor` 当前结果：在这次文档化的烟雾规模运行中，这三项维度上都不是领先者

## 当前边界

当前证据被刻意限定在较窄范围内。
它**还不能**建立已验证的 FASTQ 专用同类排名，因为 `Spring` 仍被标记为 deferred，而不是 measured。
因此这里应被视为仓库基于该 smoke-scale 运行当前可证明内容的审计线索，而不是无条件的 best-in-class 结论。

## 下一步可阅读

- [白皮书](/zh/whitepaper/)：查看实测结果如何约束公开主张
- [资源](/zh/resources/)：查看仓库路径与外部链接
