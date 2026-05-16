---
layout: home

hero:
  name: fq-compressor
  text: FASTQ Compression Whitepaper
  tagline: Inspect the algorithmic thesis, system boundaries, evidence chain, and random-access archive design in one bilingual technical portal.
  actions:
    - theme: brand
      text: Read the whitepaper
      link: ./whitepaper/
    - theme: alt
      text: Inspect architecture
      link: ./architecture/
    - theme: alt
      text: Review evidence
      link: ./benchmarks/
    - theme: alt
      text: Operator academy
      link: ./academy/
---

<EvidenceGrid locale="en" />

<ArchitectureAtlas locale="en" />

## What the site argues

fq-compressor is documented here as a technical argument, not a marketing page. A useful FASTQ compressor is not
defined by a single ratio chart. It is defined by how well compression density, throughput, random-access semantics,
format design, and repository-verifiable evidence fit together.

```mermaid
flowchart LR
    A[FASTQ input] --> B[Parser and compressed stream I/O]
    B --> C[Global analyzer and reorder planning]
    C --> D[Block compression and stream transforms]
    D --> E[FQC writer and index]
    E --> F[Verify, info, range decode, restore]
```

The reading order is deliberate: first understand the thesis, then the boundaries that make the thesis implementable,
then the evidence that constrains what may be claimed publicly.

<ReadingTracks locale="en" />

<CitationCluster locale="en" />
