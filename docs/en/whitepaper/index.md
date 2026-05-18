# Algorithms whitepaper

The whitepaper lane explains the compression thesis behind fq-compressor without turning the public docs into an implementation notebook. Read it as a systems argument about why short-read redundancy, block structure, and retrieval semantics can coexist in one archive design.

<div class="wp-grid wp-grid--two">
  <article class="wp-card">
    <p class="wp-kicker">Central thesis</p>
    <p>fq-compressor gets its identity from the interaction between assembly-inspired sequence reduction, dedicated quality modeling, and an archive format that keeps direct lookup viable.</p>
  </article>
  <article class="wp-card">
    <p class="wp-kicker">Boundary condition</p>
    <p>The whitepaper is intentionally paired with the performance section. Design claims may be ambitious, but public positioning is kept inside the evidence surface the repository can reproduce today.</p>
  </article>
</div>

## Dossier map

| Dossier | Core question | Why it matters |
| --- | --- | --- |
| [ABC pipeline](./abc-pipeline) | How are reads grouped and reduced before archive write-out? | Explains the main sequence-side compression lever |
| [SCM quality modeling](./scm-quality) | Why are quality values handled as their own probabilistic stream? | Shows why ratio and throughput trade-offs are not sequence-only |
| [Read reordering](./reordering) | Why change input order if the archive must remain reversible? | Frames locality as a compression tool rather than a preprocessing trick |
| [Consensus and delta representation](./consensus-delta) | Why encode one local consensus plus sparse edits? | Connects assembly-style reasoning to block-local storage |

## Questions this section should answer

- Why does fq-compressor treat read ordering as a first-class operation rather than a hidden optimization?
- Why are sequence, ID, and quality transforms separated instead of collapsed into one opaque codec?
- Which pieces of the design are inherited from literature such as SPRING [R1], and which are specific to fq-compressor's archive contract?

## Continue with

- [System Design](/en/architecture/) for module boundaries and archive responsibilities
- [Performance](/en/benchmarks/) for the current public proof surface
- [References](/en/research/references) for the bibliography and comparator shelf
