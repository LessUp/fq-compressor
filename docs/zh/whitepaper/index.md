# 算法白皮书

白皮书路径解释 fq-compressor 的压缩命题，但不会把公开文档写成实现细节笔记。你可以把它当作一份系统论证，说明短读长冗余、block 结构与检索语义如何同时落在一个归档设计里。

<div class="wp-grid wp-grid--two">
  <article class="wp-card">
    <p class="wp-kicker">中心命题</p>
    <p>fq-compressor 的身份来自 assembly 风格序列压缩、独立质量值建模，以及保持直接检索能力的归档格式三者之间的耦合。</p>
  </article>
  <article class="wp-card">
    <p class="wp-kicker">边界条件</p>
    <p>白皮书被刻意和性能证据部分绑定在一起。设计主张可以有野心，但公开定位必须停留在仓库今天可复现的证明面内。</p>
  </article>
</div>

## 分卷地图

| 分卷 | 核心问题 | 为什么重要 |
| --- | --- | --- |
| [ABC 流水线](./abc-pipeline) | reads 如何在落盘前被分组和压缩表示？ | 它解释序列侧最主要的压缩杠杆 |
| [SCM 质量值建模](./scm-quality) | 为什么质量值要被当作独立概率流？ | 它说明压缩比和吞吐并不只由序列决定 |
| [Reads 重排](./reordering) | 如果归档必须可逆，为什么还要改输入顺序？ | 它把局部性从 preprocessing 小技巧提升为压缩工具 |
| [共识与差分表示](./consensus-delta) | 为什么用一条局部共识加稀疏编辑表示多条 reads？ | 它把 assembly 风格推理和 block 级存储连接起来 |

## 这一节应该回答什么

- 为什么 fq-compressor 把 read ordering 当作一等操作，而不是隐藏优化？
- 为什么序列、ID、质量值的变换要拆开，而不是塞进一个黑箱 codec？
- 哪些设计直接继承自 SPRING [R1] 等文献，哪些部分则属于 fq-compressor 自己的归档契约？

## 下一步

- [系统设计](/zh/architecture/)：看模块边界和归档职责
- [性能证据](/zh/benchmarks/)：看公开证明面
- [书目与仓库](/zh/research/references)：看参考文献和对照对象
