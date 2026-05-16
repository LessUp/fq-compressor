# References

Use this page as the bibliography for the public documentation set.

## Papers

| Topic | Reference | Why it matters here |
| --- | --- | --- |
| Assembly-based compression | Chandak et al., [SPRING: a next-generation compressor for FASTQ files](https://doi.org/10.1093/bioinformatics/btz074) | Establishes the closest paper-level framing for ABC-style ordering and consensus work |
| Long-read comparison | Sahlin et al., [NanoSpring: reference-free lossless compression of nanopore sequencing reads](https://doi.org/10.1101/2021.06.09.447198) | Useful when explaining why fq-compressor is not optimized around nanopore-first assumptions |

## Repositories

| Repository | Role in the reading path |
| --- | --- |
| [shubhamchandak94/Spring](https://github.com/shubhamchandak94/Spring) | Closest external reference for the assembly-based compression framing |
| [jkbonfield/fqzcomp](https://github.com/jkbonfield/fqzcomp) | Compact quality-value compression reference |
| [shubhamchandak94/HARC](https://github.com/shubhamchandak94/HARC) | Another FASTQ-specialized comparator |
| [qm2/NanoSpring](https://github.com/qm2/NanoSpring) | Long-read comparison target |

## Repository-local study anchors

- [`vendor/spring-core/`](https://github.com/LessUp/fq-compressor/tree/master/vendor/spring-core)
- [`ref-projects/`](https://github.com/LessUp/fq-compressor/tree/master/ref-projects)
- [`benchmark/results/`](https://github.com/LessUp/fq-compressor/tree/master/benchmark/results)
- [`openspec/specs/`](https://github.com/LessUp/fq-compressor/tree/master/openspec/specs)
