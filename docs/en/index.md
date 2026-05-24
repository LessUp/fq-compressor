---
layout: home
titleTemplate: fq-compressor - FASTQ Compression

head:
  - - meta
    - name: description
      content: FASTQ compression whitepaper, architecture, and evidence portal

hero:
  name: fq-compressor
  text: Systems Whitepaper for FASTQ Compression
  tagline: Audit the algorithmic thesis, archive contract, performance boundary, and reference shelf behind fq-compressor in one bilingual portal.
  actions:
    - theme: brand
      text: Read algorithms
      link: ./whitepaper/
    - theme: alt
      text: Inspect system design
      link: ./architecture/
    - theme: alt
      text: Review performance
      link: ./benchmarks/
    - theme: alt
      text: Open references
      link: ./research/
---

<div class="wp-section">
  <div class="wp-abstract">
    <p class="wp-kicker">Abstract</p>
    <h2>Compression ratio matters here because it is tied to retrieval semantics, evidence provenance, and code boundaries.</h2>
    <p class="wp-lead">fq-compressor is documented as a coupled system. Read ordering, block-local transforms, FQC indexing, benchmark methodology, and operational workflows are treated as one public contract.</p>
  </div>

  <div class="wp-grid wp-grid--two">
    <article class="wp-card">
      <p class="wp-kicker">What you can audit here</p>
      <ul class="wp-list">
        <li>Algorithm framing for ABC, SCM, reversible reordering, and consensus-plus-delta encoding</li>
        <li>System design for block-local compression, archive materialization, and O(1) random access</li>
        <li>Performance language bounded by tracked benchmark artifacts and explicit methodology</li>
      </ul>
    </article>
    <article class="wp-card">
      <p class="wp-kicker">Primary repository anchors</p>
      <ul class="wp-list">
        <li><a href="./architecture/">Architecture</a> maps public concepts to <code>include/fqc/</code>, <code>src/</code>, and format responsibilities</li>
        <li><a href="./benchmarks/">Performance</a> keeps benchmark claims tied to repository artifacts and proof limits</li>
        <li><a href="./research/">References</a> connects the site to papers, comparator repositories, and archived notes</li>
      </ul>
    </article>
  </div>
</div>

<EvidenceGrid locale="en" />

<ArchitectureAtlas locale="en" />

<div class="wp-section">
  <div class="wp-grid wp-grid--three">
    <article class="wp-card">
      <p class="wp-kicker">Algorithms</p>
      <h3>ABC plus SCM, framed as a system thesis</h3>
      <p>The whitepaper lane explains why fq-compressor splits read ordering, consensus-style sequence reduction, and quality modeling into distinct but cooperating stages.</p>
    </article>
    <article class="wp-card">
      <p class="wp-kicker">Evidence</p>
      <h3>Performance claims stay narrower than aspiration</h3>
      <p>The benchmark lane is intentionally conservative. It shows what the repository can prove today, not every future claim the project may eventually support.</p>
    </article>
    <article class="wp-card">
      <p class="wp-kicker">References</p>
      <h3>The portal cites literature and comparators explicitly</h3>
      <p>The reference shelf ties fq-compressor back to SPRING [R1], fqzcomp [R2], HARC [C2], NanoSpring [R3], and repository-local evidence anchors.</p>
    </article>
  </div>
</div>

<ReadingTracks locale="en" />

<CitationCluster locale="en" />
