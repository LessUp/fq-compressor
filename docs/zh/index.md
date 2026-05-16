---
layout: home

hero:
  name: fq-compressor
  text: FASTQ 压缩白皮书
  tagline: 在一个双语技术站点里同时检查算法命题、系统边界、证据链，以及随机访问归档设计。
  actions:
    - theme: brand
      text: 阅读白皮书
      link: ./whitepaper/
    - theme: alt
      text: 查看架构
      link: ./architecture/
    - theme: alt
      text: 查看证据
      link: ./benchmarks/
    - theme: alt
      text: 进入学院
      link: ./academy/
---

<EvidenceGrid locale="zh" />

<ArchitectureAtlas locale="zh" />

## 这个站点想论证什么

这里把 fq-compressor 当作一份技术论证来呈现，而不是营销页。一个真正可用的 FASTQ 压缩器，不应该只靠一张压缩率图表成立。它必须让压缩密度、吞吐、随机访问语义、格式设计与仓库可追溯证据彼此咬合。

```mermaid
flowchart LR
    A[FASTQ 输入] --> B[解析器与压缩流 I/O]
    B --> C[全局分析与重排规划]
    C --> D[block 压缩与逻辑流变换]
    D --> E[FQC writer 与索引]
    E --> F[verify、info、范围解码、恢复]
```

这里的阅读顺序是刻意安排过的：先理解命题，再理解让命题可落地的边界，最后回到约束公开主张的证据链。

<ReadingTracks locale="zh" />

<CitationCluster locale="zh" />
