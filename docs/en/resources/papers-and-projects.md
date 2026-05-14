# Papers and projects

This page is a curated reading list, not a claim ladder. Use it to understand where fq-compressor borrows ideas, where it compares itself, and which external codebases are worth reading next.

## Papers

### SPRING: a next-generation compressor for FASTQ files

- Paper: [Bioinformatics, 2019](https://doi.org/10.1093/bioinformatics/btz074)
- Project: [shubhamchandak94/Spring](https://github.com/shubhamchandak94/Spring)
- Why it matters here: fq-compressor reuses the assembly-based compression framing and keeps a vendored learning-oriented extraction in [`vendor/spring-core/`](https://github.com/LessUp/fq-compressor/tree/master/vendor/spring-core).

### NanoSpring: reference-free lossless compression of nanopore sequencing reads

- Paper: [bioRxiv preprint](https://doi.org/10.1101/2021.06.09.447198)
- Project: [qm2/NanoSpring](https://github.com/qm2/NanoSpring)
- Why it matters here: useful contrast for long-read and nanopore-focused design choices rather than short-read random-access workflows.

## Reference implementations

### Directly relevant FASTQ compressors

- [Spring](https://github.com/shubhamchandak94/Spring): the clearest external reference for ABC-style read ordering, consensus building, and delta encoding.
- [fqzcomp](https://github.com/jkbonfield/fqzcomp): a compact quality-compression reference, useful when reasoning about quality modeling trade-offs.
- [HARC](https://github.com/shubhamchandak94/HARC): another specialized FASTQ compressor worth reading when comparing architecture and data-path choices.
- [NanoSpring](https://github.com/qm2/NanoSpring): a long-read comparator, especially for nanopore-oriented assumptions.

### Repository-local comparison anchors

- [`ref-projects/README.md`](https://github.com/LessUp/fq-compressor/blob/master/ref-projects/README.md): explains which external tools are tracked as comparison or study targets.
- [`vendor/spring-core/README.md`](https://github.com/LessUp/fq-compressor/blob/master/vendor/spring-core/README.md): documents the extracted Spring components and their license boundary.
- [`docs/benchmark/README.md`](https://github.com/LessUp/fq-compressor/blob/master/docs/benchmark/README.md): the benchmark narrative that constrains public comparison claims.

## How to use this list

- Read the [Whitepaper](/en/whitepaper/) first if you need the fq-compressor thesis.
- Use this page when you want neighboring implementations and papers, not product instructions.
- Treat archived notes in `docs/archive/` as historical background only.
