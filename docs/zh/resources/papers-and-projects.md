# 论文与项目

这一页是整理过的阅读清单，不是“谁更强”的排行榜。它的作用是帮助你看清 fq-compressor 借鉴了哪些思路、参考了哪些对照项目，以及下一步值得阅读哪些外部实现。

## 论文

### SPRING: a next-generation compressor for FASTQ files

- 论文：[Bioinformatics, 2019](https://doi.org/10.1093/bioinformatics/btz074)
- 项目：[shubhamchandak94/Spring](https://github.com/shubhamchandak94/Spring)
- 与本仓库的关系：fq-compressor 沿用了 assembly-based compression 的问题框架，并在 [`vendor/spring-core/`](https://github.com/LessUp/fq-compressor/tree/master/vendor/spring-core) 中保留了一个面向学习和对照的提取版本。

### NanoSpring: reference-free lossless compression of nanopore sequencing reads

- 论文：[bioRxiv 预印本](https://doi.org/10.1101/2021.06.09.447198)
- 项目：[qm2/NanoSpring](https://github.com/qm2/NanoSpring)
- 与本仓库的关系：它更适合作为长读长、纳米孔场景下的对照，而不是短读长随机访问工作流的直接替身。

## 参考实现

### 与 FASTQ 压缩直接相关的项目

- [Spring](https://github.com/shubhamchandak94/Spring)：最适合对照 ABC 风格的 read 重排、共识构建与差分编码。
- [fqzcomp](https://github.com/jkbonfield/fqzcomp)：一个紧凑的质量值压缩参考，有助于理解质量建模取舍。
- [HARC](https://github.com/shubhamchandak94/HARC)：另一个 FASTQ 专用压缩器，适合做架构和数据路径对照。
- [NanoSpring](https://github.com/qm2/NanoSpring)：长读长方向的对照项目，特别适合纳米孔假设下的阅读。

### 仓库内的对照入口

- [`ref-projects/README.md`](https://github.com/LessUp/fq-compressor/blob/master/ref-projects/README.md)：说明本仓库跟踪了哪些外部工具作为比较或学习对象。
- [`vendor/spring-core/README.md`](https://github.com/LessUp/fq-compressor/blob/master/vendor/spring-core/README.md)：说明提取的 Spring 组件及其许可边界。
- [`docs/benchmark/README.md`](https://github.com/LessUp/fq-compressor/blob/master/docs/benchmark/README.md)：当前公开 benchmark 叙述，决定了仓库可以对外提出哪些比较结论。

## 如何使用这份清单

- 如果你先想理解 fq-compressor 的论点，请先读 [白皮书](/zh/whitepaper/)。
- 如果你想找相邻论文和实现，而不是操作说明，就读这一页。
- `docs/archive/` 中的材料只适合做历史背景，不是当前对外叙述。
