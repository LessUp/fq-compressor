---
layout: home

hero:
  name: fq-compressor
  text: FASTQ 压缩系统白皮书
  tagline: 在一个双语站点里同时审阅 fq-compressor 的算法命题、归档契约、性能边界，以及参考研究。
  actions:
    - theme: brand
      text: 阅读算法白皮书
      link: ./whitepaper/
    - theme: alt
      text: 查看系统设计
      link: ./architecture/
    - theme: alt
      text: 查看性能证据
      link: ./benchmarks/
    - theme: alt
      text: 打开参考研究
      link: ./research/
---

<div class="wp-section">
  <div class="wp-abstract">
    <p class="wp-kicker">摘要</p>
    <h2>这里谈压缩率，不是因为它够响亮，而是因为它和检索语义、证据出处、代码边界绑在一起。</h2>
    <p class="wp-lead">fq-compressor 被呈现为一个耦合系统。read ordering、block 级变换、FQC 索引、benchmark 方法学与操作路径，被当作同一份公开契约来叙述。</p>
  </div>

  <div class="wp-grid wp-grid--two">
    <article class="wp-card">
      <p class="wp-kicker">这里能审计什么</p>
      <ul class="wp-list">
        <li>ABC、SCM、可逆重排、共识加差分编码的算法框架</li>
        <li>block 级压缩、归档落盘与 O(1) 随机访问的系统设计</li>
        <li>被 benchmark 产物和明确方法学边界约束住的性能叙事</li>
      </ul>
    </article>
    <article class="wp-card">
      <p class="wp-kicker">主要仓库锚点</p>
      <ul class="wp-list">
        <li><a href="./architecture/">系统设计</a>把公开概念对应到 <code>include/fqc/</code>、<code>src/</code> 与格式职责</li>
        <li><a href="./benchmarks/">性能证据</a>把 benchmark 表述钉回仓库产物和证明边界</li>
        <li><a href="./research/">参考研究</a>把站点接到论文、对照仓库与 OpenSpec</li>
      </ul>
    </article>
  </div>
</div>

<EvidenceGrid locale="zh" />

<ArchitectureAtlas locale="zh" />

<div class="wp-section">
  <div class="wp-grid wp-grid--three">
    <article class="wp-card">
      <p class="wp-kicker">算法</p>
      <h3>ABC 与 SCM 被写成一条系统命题</h3>
      <p>白皮书路径解释 fq-compressor 为什么要把 read ordering、共识化序列表示和质量值建模拆成不同但协作的阶段。</p>
    </article>
    <article class="wp-card">
      <p class="wp-kicker">证据</p>
      <h3>公开主张必须比愿景更窄</h3>
      <p>性能部分刻意保守。它展示仓库今天能证明什么，而不是项目未来也许能支持的一切大结论。</p>
    </article>
    <article class="wp-card">
      <p class="wp-kicker">参考</p>
      <h3>论文和对照仓库被显式纳入叙事</h3>
      <p>参考研究把 SPRING [R1]、fqzcomp [R2]、HARC [C2]、NanoSpring [R3] 与本地证据锚点并排放在台面上。</p>
    </article>
  </div>
</div>

<ReadingTracks locale="zh" />

<CitationCluster locale="zh" />
