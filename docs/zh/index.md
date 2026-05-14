---
layout: home

hero:
  name: fq-compressor
  text: 技术入口
  tagline: 把 FASTQ 压缩理解为一个工程系统，而不只是一个基准测试结论。
  actions:
    - theme: brand
      text: 阅读白皮书
      link: ./whitepaper/
    - theme: alt
      text: 查看架构
      link: ./architecture/
    - theme: alt
      text: English
      link: ../en/
    - theme: alt
      text: GitHub
      link: https://github.com/LessUp/fq-compressor
---

<script setup>
import { withBase } from 'vitepress'
</script>

<MetricStrip locale="zh" />

<div class="portal-shell">
  <section class="portal-section">
    <div class="portal-prose">
      <div>
        <p class="portal-kicker">门户命题</p>
        <h2>只有当周围系统可被检查时，压缩结论才真正有意义。</h2>
      </div>
      <p class="portal-lead">
        这里把 fq-compressor 定位为一份技术白皮书入口，而不是只展示一张压缩率榜单。
        真正关键的不只是“压了多少”，还包括归档格式在检索、校验、运维约束下的整体表现。
      </p>
      <p>
        因此，这个站点把格式设计、基准方法、命令工作流和仓库证据视为一条连续叙事。
        如果某个主张无法回溯到架构或测量方法，它就不应该被当成项目的稳定属性来传播。
      </p>
    </div>
    <div class="portal-columns">
      <article class="portal-panel">
        <h3>这里重点呈现什么</h3>
        <ul>
          <li>把压缩率与吞吐、访问语义、实现边界放在同一张图里理解。</li>
          <li>让首页摘要可以一路追到基准产物与源码，而不是停在口号层。</li>
          <li>给评估者、操作者与贡献者一条更短的阅读路径。</li>
        </ul>
      </article>
      <article class="portal-panel">
        <h3>这里刻意避免什么</h3>
        <ul>
          <li>只有 headline 数字，却没有数据集、方法与访问成本解释的说法。</li>
          <li>跑在仓库证据前面的营销化表达。</li>
          <li>重复归档中的探索性材料，而不是明确指向当前生效的事实来源。</li>
        </ul>
      </article>
    </div>
  </section>
</div>

<KnowledgeMap locale="zh" />

<div class="portal-shell">
  <section class="portal-section">
    <div class="portal-section__header">
      <div>
        <p class="portal-kicker">阅读路径</p>
        <h2>按当前问题选择最短路径，而不是被迫完整通读。</h2>
      </div>
      <p class="portal-lead">
        这个入口支持按需阅读。先从最窄的问题开始，只有在需要更多把握时才继续展开。
      </p>
    </div>
    <div class="portal-paths">
      <ul>
        <li>
          <strong>我想一次看完核心主张</strong>
          先读 <a :href="withBase('/zh/whitepaper/')">白皮书</a>，再去 <a :href="withBase('/zh/benchmarks/')">基准测试</a>确认数字。
        </li>
        <li>
          <strong>我想理解实现模型</strong>
          带着 <a :href="withBase('/zh/overview/')">概览</a> 的背景去读 <a :href="withBase('/zh/architecture/')">架构</a>。
        </li>
        <li>
          <strong>我想运行或验证工具</strong>
          直接进入 <a :href="withBase('/zh/guides/')">指南</a>，并结合 <a :href="withBase('/zh/resources/')">资源</a> 查看源码入口。
        </li>
        <li>
          <strong>我需要先确认出处再相信一句话</strong>
          从 <a :href="withBase('/zh/benchmarks/')">基准测试</a> 出发，继续追到仓库中的结果文件与规范链接。
        </li>
      </ul>
    </div>
  </section>
</div>

<TopicCardGrid locale="zh" />
