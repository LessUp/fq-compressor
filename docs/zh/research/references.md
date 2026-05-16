# 参考文献

把这一页当作公开文档集的参考文献表。

## 论文

| 主题 | 参考文献 | 为什么和这里相关 |
| --- | --- | --- |
| assembly-based compression | Chandak 等， [SPRING: a next-generation compressor for FASTQ files](https://doi.org/10.1093/bioinformatics/btz074) | 是 ABC 风格 read ordering 与 consensus 推理最接近的论文框架 |
| 长读长对照 | Sahlin 等， [NanoSpring: reference-free lossless compression of nanopore sequencing reads](https://doi.org/10.1101/2021.06.09.447198) | 有助于解释 fq-compressor 为什么没有按 nanopore-first 假设来组织 |

## 仓库

| 仓库 | 在阅读路径中的角色 |
| --- | --- |
| [shubhamchandak94/Spring](https://github.com/shubhamchandak94/Spring) | assembly-based compression 框架最接近的外部参考 |
| [jkbonfield/fqzcomp](https://github.com/jkbonfield/fqzcomp) | 紧凑的质量值压缩参考 |
| [shubhamchandak94/HARC](https://github.com/shubhamchandak94/HARC) | 另一个 FASTQ 专用对照对象 |
| [qm2/NanoSpring](https://github.com/qm2/NanoSpring) | 长读长方向对照 |

## 仓库内研究锚点

- [`vendor/spring-core/`](https://github.com/LessUp/fq-compressor/tree/master/vendor/spring-core)
- [`ref-projects/`](https://github.com/LessUp/fq-compressor/tree/master/ref-projects)
- [`benchmark/results/`](https://github.com/LessUp/fq-compressor/tree/master/benchmark/results)
- [`openspec/specs/`](https://github.com/LessUp/fq-compressor/tree/master/openspec/specs)
