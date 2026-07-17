# Changelog

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.3.0-rc1] - 2026-07-17

### Added

- FQC v2 sequential little-endian archive with independent ID, packed-sequence, and quality
  streams; per-frame/global XXH64 integrity checks; strict allocation bounds; and an EOS footer.
- One bounded engine for compression, decompression, and full verification.
- Explicit/automatic Illumina, ONT, PacBio HiFi, and PacBio CLR profile metadata.
- Single/paired FASTQ support, direct `.gz` input, stdin/stdout pipelines, and exact IUPAC/lowercase
  preservation.
- Production scaling, randomised throughput, memory-budget, corruption, and real CLI process tests.

### Changed

- Replaced the CLI with the minimal `compress`, `decompress`, and `verify` v2 surface.
- Default memory budget is 16 GiB; minimum supported budget is 64 MiB. Frame accumulation and both
  encode/decode paths apply conservative byte-based admission.
- Paired files are stored and restored as one interleaved sequence of complete pairs.
- FASTQ validation now honours `ParserOptions::validBases` and defaults to the full uppercase and
  lowercase IUPAC nucleotide alphabet.
- Gzip input remains streaming and now accepts standards-compliant concatenated gzip members.
- Readers reject data appended after the authenticated v2 footer; regular-file output is staged
  and uses atomic replacement where the platform supports it.

### Removed

- FQC v1 reader/writer, index, random access, reorder map, and all v1 compatibility.
- Assembly-based compression, dense SCM quality coding, lossy modes, and the misleading overlap
  codec surface.
- Duplicate serial/TBB pipeline implementations, oneTBB dependency, async-I/O test island, and
  unused paired-parser layer.
- `info`, range extraction, split-PE output, original-order restoration, thread flags, and other
  legacy CLI options.

### Performance

- Pinned three-run medians on the available 8-core x86_64 WSL2 host measured 53.15/182.40 MiB/s
  for randomised 150 bp data and 55.66/215.22 MiB/s for randomised 20 kbp data
  (compress/decompress) under a 64 MiB configured budget. WSL2 wall-clock noise has produced lower
  reruns, so these are evidence snapshots rather than stable guarantees.
- The local performance harness defaults to three repetitions, reports median throughput and
  maximum RSS, and can enforce the 50/100 MiB/s release floor.
- Specialised codecs remain gated and are not shipped without adequate real-data evidence of at
  least 5% median size improvement, no dataset regression above 1%, and the throughput floor.

## [0.1.0] - 2026-04-16

### Added

- Initial public C++ implementation.

[Unreleased]: https://github.com/LessUp/fq-compressor/compare/v0.3.0-rc1...HEAD
[0.3.0-rc1]: https://github.com/LessUp/fq-compressor/compare/v0.2.0...v0.3.0-rc1
[0.1.0]: https://github.com/LessUp/fq-compressor/releases/tag/v0.1.0
