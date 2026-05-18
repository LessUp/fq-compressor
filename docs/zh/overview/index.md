# 导读

把这页当成整个站点的阅读契约。它定义 fq-compressor 想在公开层面证明什么、文档如何组织，以及不同读者最快该走哪条路线。

<div class="wp-grid wp-grid--two">
  <article class="wp-card">
    <p class="wp-kicker">站点契约</p>
    <ul class="wp-list">
      <li>本站把 fq-compressor 当作系统设计来论证，而不是单独的一条 benchmark headline。</li>
      <li>每条稳定的公开主张，都应该能回到架构、方法学或仓库中的跟踪产物。</li>
      <li>阅读顺序优先服务严苛的 GitHub 高级开发者和技术评审。</li>
    </ul>
  </article>
  <article class="wp-card">
    <p class="wp-kicker">它不是什么</p>
    <ul class="wp-list">
      <li>不是 README 镜像</li>
      <li>不是脱离证据的功能清单</li>
      <li>不是靠复杂前端效果掩盖工程内容的展示页</li>
    </ul>
  </article>
</div>

## 默认阅读顺序

1. 先读 [算法白皮书](/zh/whitepaper/) 理解压缩命题。
2. 再读 [系统设计](/zh/architecture/) 查看这些思想分别落在哪些模块和格式责任上。
3. 再读 [性能证据](/zh/benchmarks/) 确认仓库当前实际能证明什么，以及证据边界停在哪里。
4. 如果问题偏实操，就进入 [操作路径](/zh/academy/)。
5. 如果问题偏文献、对照或历史，就进入 [参考研究](/zh/research/)。

## 读者意图

| 读者 | 从哪里开始 | 接着读什么 | 主要结果 |
| --- | --- | --- | --- |
| 高级评审 | [算法白皮书](/zh/whitepaper/) | [性能证据](/zh/benchmarks/) | 判断公开叙事是否技术上成立 |
| 贡献者 | [系统设计](/zh/architecture/) | [操作路径](/zh/academy/) | 动手前先理解边界 |
| 操作者 | [操作路径](/zh/academy/) | [系统设计](/zh/architecture/) | 运行和校验归档时不靠猜 |
| 研究读者 | [参考研究](/zh/research/) | [算法白皮书](/zh/whitepaper/) | 把 fq-compressor 放回 FASTQ 压缩的大语境 |

## 为什么这样组织

fq-compressor 只有在文档像一份技术 dossier 时才更容易被信任。于是顶层路径被保持得足够浅，证据话术被显式约束，而支撑性的书目和对照仓库也不会被藏进脚注里。

<ReadingTracks locale="zh" />
