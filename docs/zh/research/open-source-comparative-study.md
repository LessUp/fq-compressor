# 开源项目对照

这一页不是排行榜，而是设计对照表：哪些外部系统塑造了 fq-compressor，哪些取舍被继承，哪些比较结论又受仓库当前证据边界约束。

## 对照框架

| 项目 | 它在讨论中贡献了什么 | fq-compressor 为什么研究它 | 关键差异 |
| --- | --- | --- | --- |
| [SPRING](https://github.com/shubhamchandak94/Spring) | assembly-based compression、read ordering、consensus-and-delta 框架 | 它是 ABC 风格推理最直接的上游参考 | fq-compressor 把随机访问归档语义放进公开叙事中心 |
| [fqzcomp](https://github.com/jkbonfield/fqzcomp) | 紧凑的质量值建模 | 有助于理解质量编码如何影响压缩比与吞吐 | fq-compressor 把质量值建模视为更大归档设计中的一条逻辑流 |
| [HARC](https://github.com/shubhamchandak94/HARC) | 另一种 FASTQ 专用压缩器 | 适合对照架构和范围选择 | fq-compressor 对证明边界的文档化更明确 |
| [NanoSpring](https://github.com/qm2/NanoSpring) | 长读长、纳米孔假设 | 适合解释 fq-compressor 没有优先优化什么 | fq-compressor 主要围绕短读长、可索引检索工作流来叙述 |

## 仓库内锚点

- [`ref-projects/README.md`](https://github.com/LessUp/fq-compressor/blob/master/ref-projects/README.md)：说明跟踪了哪些对照工具。
- [`vendor/spring-core/README.md`](https://github.com/LessUp/fq-compressor/blob/master/vendor/spring-core/README.md)：说明提取的 Spring 组件及许可边界。
- [`docs/benchmark/README.md`](https://github.com/LessUp/fq-compressor/blob/master/docs/benchmark/README.md)：限制仓库当前可以公开支撑哪些比较结论。

## fq-compressor 保留了什么

- 把 read ordering 和局部相似性视为一等压缩杠杆。
- 明确格式职责，让随机访问不是事后补丁。
- 把压缩故事写成流水线，而不是黑箱。

## fq-compressor 明确拒绝什么

- 超出仓库实测证据范围的公开主张。
- 只看压缩率、不看检索成本与归档语义的理解方式。
- 在 closeout mode 下继续无限扩张产品面。

## 下一步

- [参考文献](./references)：按主题查看论文与仓库
- [演进思考](./evolution-notes)：理解当前项目姿态
