---
layout: home

hero:
  name: fq-compressor
  text: Technical Portal
  tagline: FASTQ compression as an engineering system, not just a benchmark claim.
  actions:
    - theme: brand
      text: Read the whitepaper
      link: ./whitepaper/
    - theme: alt
      text: Inspect architecture
      link: ./architecture/
    - theme: alt
      text: 中文
      link: ../zh/
    - theme: alt
      text: GitHub
      link: https://github.com/LessUp/fq-compressor
---

<script setup>
import { withBase } from 'vitepress'
</script>

<MetricStrip locale="en" />

<div class="portal-shell">
  <section class="portal-section">
    <div class="portal-prose">
      <div>
        <p class="portal-kicker">Portal thesis</p>
        <h2>Compression claims become useful only when the system around them is inspectable.</h2>
      </div>
      <p class="portal-lead">
        fq-compressor is presented here as a whitepaper portal for readers who want more than a ratio chart.
        The important question is not only how much data can be reduced, but how the archive behaves under real
        retrieval, verification, and maintenance constraints.
      </p>
      <p>
        This site therefore treats format design, benchmark method, command workflows, and repository evidence as
        one connected narrative. If a claim cannot be traced to architecture or measurement, it should not be
        treated as a durable property of the project.
      </p>
    </div>
    <div class="portal-columns">
      <article class="portal-panel">
        <h3>What this portal centers</h3>
        <ul>
          <li>Compression ratio alongside throughput, access semantics, and implementation boundaries.</li>
          <li>Evidence that can be followed from homepage summary into benchmark artifacts and source code.</li>
          <li>A reading order that helps operators, evaluators, and contributors reach the right section quickly.</li>
        </ul>
      </article>
      <article class="portal-panel">
        <h3>What this portal avoids</h3>
        <ul>
          <li>Headline numbers with no explanation of datasets, measurement method, or retrieval cost.</li>
          <li>Marketing language that outruns the repository's current proof surface.</li>
          <li>Duplicating archived exploratory material instead of pointing to current sources of truth.</li>
        </ul>
      </article>
    </div>
  </section>
</div>

<KnowledgeMap locale="en" />

<div class="portal-shell">
  <section class="portal-section">
    <div class="portal-section__header">
      <div>
        <p class="portal-kicker">Reader paths</p>
        <h2>Use the shortest route that answers your current question.</h2>
      </div>
      <p class="portal-lead">
        The portal is designed for selective reading. Start narrow, then widen only when you need more confidence.
      </p>
    </div>
    <div class="portal-paths">
      <ul>
        <li>
          <strong>I need the claim in one pass</strong>
          Start with <a :href="withBase('/en/whitepaper/')">Whitepaper</a>, then confirm numbers in
          <a :href="withBase('/en/benchmarks/')">Benchmarks</a>.
        </li>
        <li>
          <strong>I need the implementation model</strong>
          Read <a :href="withBase('/en/architecture/')">Architecture</a> after the
          <a :href="withBase('/en/overview/')">Overview</a> for context.
        </li>
        <li>
          <strong>I need to run or verify the tool</strong>
          Jump to <a :href="withBase('/en/guides/')">Guides</a> and keep
          <a :href="withBase('/en/resources/')">Resources</a> nearby for source references.
        </li>
        <li>
          <strong>I need provenance before I trust a statement</strong>
          Trace the path from <a :href="withBase('/en/benchmarks/')">Benchmarks</a> into repository artifacts and linked specs.
        </li>
      </ul>
    </div>
  </section>
</div>

<TopicCardGrid locale="en" />
