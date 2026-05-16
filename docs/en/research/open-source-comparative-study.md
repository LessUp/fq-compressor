# Open-source comparative study

This page is not a leaderboard. It is a design comparison: which external systems shape fq-compressor, which trade-offs
it inherits, and where the repository currently limits the strength of comparison claims.

## Comparative frame

| Project | What it contributes to the conversation | Why fq-compressor studies it | Important difference |
| --- | --- | --- | --- |
| [SPRING](https://github.com/shubhamchandak94/Spring) | Assembly-based compression, read ordering, consensus-and-delta framing | The clearest upstream reference for ABC-style reasoning | fq-compressor centers random-access archive semantics as part of the public story |
| [fqzcomp](https://github.com/jkbonfield/fqzcomp) | Compact quality-value modeling | Useful for understanding how quality coding changes ratio and throughput | fq-compressor treats quality modeling as one stream inside a wider archive design |
| [HARC](https://github.com/shubhamchandak94/HARC) | Another specialized FASTQ compressor | Good comparator for architecture and scope choices | fq-compressor documents proof boundaries more explicitly |
| [NanoSpring](https://github.com/qm2/NanoSpring) | Long-read and nanopore assumptions | Useful contrast when explaining what fq-compressor is not optimizing for first | fq-compressor is framed primarily around short-read, indexed retrieval workflows |

## Repository-local anchors

- [`ref-projects/README.md`](https://github.com/LessUp/fq-compressor/blob/master/ref-projects/README.md): explains which external tools are tracked as comparison or study targets.
- [`vendor/spring-core/README.md`](https://github.com/LessUp/fq-compressor/blob/master/vendor/spring-core/README.md): documents the extracted Spring components and their license boundary.
- [`docs/benchmark/README.md`](https://github.com/LessUp/fq-compressor/blob/master/docs/benchmark/README.md): constrains what repository evidence can currently support in public.

## What fq-compressor keeps

- The system treats read ordering and local similarity as first-class compression levers.
- It keeps format responsibilities explicit so random access is not an afterthought.
- It documents the compression story as a pipeline, not as a monolithic black box.

## What fq-compressor rejects

- Public claims that outrun measured repository evidence.
- A purely ratio-first reading that ignores retrieval cost and archive semantics.
- Unbounded product expansion during closeout mode.

## Continue with

- [References](./references) for papers and repositories grouped by topic
- [Evolution notes](./evolution-notes) for current project posture
