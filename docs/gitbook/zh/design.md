# 设计概览

## 核心设计目标

fq-compressor 在设计上同时追求：

- 高压缩比
- 高性能并行处理
- 基于 block 的随机访问
- 文档化、可维护的工程体系

## 核心压缩路线

### Sequence

短读场景优先采用基于 Spring / Mincom 思想的 **Assembly-based Compression (ABC)**：

- minimizer bucketing
- read reordering
- local consensus
- delta encoding

### Quality

质量值采用类似 fqzcomp5 的 **SCM** 路线，重点在于：

- 上下文建模
- 自适应统计编码
- 在压缩比与速度间做平衡

### Identifier

FASTQ header 采用：

- tokenization
- delta
- 通用压缩器后端

## 为什么使用 block-based 容器

`.fqc` 的 block 设计使得项目获得以下能力：

- 随机访问
- block 独立校验
- 并行解压
- 更稳定的错误隔离

## 长读策略

本项目不强行把短读 ABC 路线扩展到所有长读数据。

对于超过 Spring 原生短读上限的数据，会切换到更稳妥的通用压缩策略，以保证：

- 可维护性
- 稳定性
- 随机访问兼容性
- 更好的工程落地成本
