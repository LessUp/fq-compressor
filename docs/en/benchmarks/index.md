# Benchmarks

The benchmarks section exists to keep public performance statements tied to reproducible evidence.
It should answer two questions: what has actually been measured in this repository, and what remains outside the current proof boundary.

## Start here

- [Benchmark methodology](/en/benchmarks/methodology) explains the evidence rules, dataset scope, and reproduction commands.
- [`docs/benchmark/README.md`](https://github.com/LessUp/fq-compressor/blob/master/docs/benchmark/README.md) is the repository-side narrative for the tracked benchmark chain.
- [`benchmark/results/err091571-local-supported.json`](https://github.com/LessUp/fq-compressor/blob/master/benchmark/results/err091571-local-supported.json) is the machine-readable tracked result.
- [`benchmark/results/err091571-local-supported.md`](https://github.com/LessUp/fq-compressor/blob/master/benchmark/results/err091571-local-supported.md) is the human-readable tracked report.

## Current evidence

The repository currently tracks one public evidence set built from ENA accession `ERR091571` using a reproducible 2,000-record smoke-scale subset.
Within that documented run, the observed leaders in the measured local-supported toolset were:

- best compression ratio: `bzip2`
- best compression speed: `bzip2`
- best decompression speed: `gzip`
- current `fq-compressor` result: not leading on those three dimensions in the documented smoke-scale run

## Current boundary

The current evidence is intentionally narrow.
It does **not** yet establish a verified specialized-peer ranking because `Spring` is still tracked as deferred rather than measured.
Readers should treat this section as an audit trail for what the repository can currently prove from that smoke-scale run, not as a blanket best-in-class conclusion.

## Continue with

- [Whitepaper](/en/whitepaper/) to see how measured results constrain public claims
- [Resources](/en/resources/) for repository paths and external links
