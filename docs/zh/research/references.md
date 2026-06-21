# 书目与仓库

把这一页当作公开文档集的引文索引。

## 编号文献

1. **[R1]** Chandak 等，[SPRING: a next-generation compressor for FASTQ files](https://doi.org/10.1093/bioinformatics/btz074)。  
   最接近 assembly-based compression、可逆重排与共识式序列压缩框架的论文来源。
2. **[R2]** Bonfield，[fqzcomp 仓库](https://github.com/jkbonfield/fqzcomp)。  
   一个紧凑的质量值压缩参考，用来迫使我们正视流级编码取舍。
3. **[R3]** Sahlin 等，[NanoSpring: reference-free lossless compression of nanopore sequencing reads](https://doi.org/10.1101/2021.06.09.447198)。  
   在解释 fq-compressor 没有首先围绕长读长场景优化时非常有用。

## 对照仓库

1. **[C1]** [shubhamchandak94/Spring](https://github.com/shubhamchandak94/Spring)  
   assembly-based compression 框架最接近的外部仓库参考。
2. **[C2]** [shubhamchandak94/HARC](https://github.com/shubhamchandak94/HARC)  
   适合比较 FASTQ 专用压缩器的架构和项目范围。
3. **[C3]** [jkbonfield/fqzcomp](https://github.com/jkbonfield/fqzcomp)  
   用来检验 fq-compressor 质量值流拆分是否合理的重要对照。
4. **[C4]** [qm2/NanoSpring](https://github.com/qm2/NanoSpring)  
   用来说明长读长方向边界的对照对象。

## 仓库内证据锚点

1. **[E1]** [`benchmark/results/`](https://github.com/LessUp/fq-compressor/tree/master/benchmark/results)  
   性能部分使用的机器可读和叙事型 benchmark 产物。
2. **[E2]** [`ref-projects/`](https://github.com/LessUp/fq-compressor/tree/master/ref-projects)  
   本地对照目标和学习记录。
3. **[E3]** [`docs/archive/`](https://github.com/LessUp/fq-compressor/tree/master/docs/archive)  
   仅保留历史研究和规格材料，供参考使用。
