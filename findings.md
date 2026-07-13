# fq-compressor v2 findings

## Confirmed current-state problems

- Pipeline execution normally bypasses global reordering while the short-read ABC implementation
  assumes similar reads are locally adjacent.
- Medium and long reads are length-prefixed ASCII followed by Zstd; there is no overlap codec.
- Quality Order-2 allocates a dense context table per worker and performs a full alphabet cumulative
  update per symbol.
- Production backpressure limits token count, not actual retained bytes; configured memory limits
  are not admission controls.
- Compression and decompression have separate single-thread and pipeline implementations.
- Existing tracked evidence is Illumina-only and does not establish third-generation suitability.

## Implementation constraints

- Repository is in closeout mode: delete/consolidate instead of preserving duplicate machinery.
- Use existing build/test scripts and C++23 style.
- Prototype code must be clearly throwaway, runnable with one command, then absorbed or removed.

## Build shape

- `fqc_core` currently owns format, algorithms, I/O, and pipeline sources; `fqc_cli` owns command
  orchestration. Tests link either core alone or core plus CLI.
- The current test suite has 16 registered CTest targets.
- Baseline `clang-debug` builds successfully. Baseline formatting is already red on the clean commit
  in `compression_profile.cpp`, `stream_factory.cpp`, and `stream_factory.h`.
- All 16 baseline tests pass in about 42 seconds.
- `FastqParser` already accepts injected streams and transparently handles file/stdin input through
  `StreamFactory`, so v2 can reuse parsing without depending on the legacy archive classes.
- The benchmark adapter is tightly coupled to legacy original-order/split-PE CLI options and must be
  updated with the v2 CLI rather than preserved.
- The existing CLI surface is large; the v2 change can materially simplify `main.cpp` once the
  fallback tracer bullet is proven.

## External evidence retained from design work

- Approximate-assembly long-read compression can require substantial time and memory; specialised
  long-read work therefore remains gated behind measured bounded prototypes.
- rANS alone does not bound model memory; sparse/static models and explicit model budgets do.

## Fallback prototype evidence

Release build, synthetic 64 MiB record sets, full ID/comment/sequence/quality round trip:

| Profile shape | Ratio | Compress MiB/s | Decompress MiB/s | Max RSS |
|---|---:|---:|---:|---:|
| 150 bp short | 2.19x | 61.1 | 187.3 | 258 MiB |
| 20 kbp long | 2.04x | 69.1 | 171.3 | 216 MiB |

- The codec-core fallback clears the 50/100 gate at 64 MiB for both shapes.
- These numbers deliberately exclude FASTQ parsing, filesystem I/O, frame coordination, checksums,
  and profile sampling. They prove a viable ceiling, not the production SLA.
- GNU `time` CPU percentages/elapsed time were inconsistent with the process steady-clock timings in
  this execution environment; retain its max RSS only and use the prototype's steady-clock figures
  for codec throughput.
- At 256 MiB, short measured 2.19x and 80.8/228.9 MiB/s; long measured 2.04x and
  110.2/369.1 MiB/s. Ratio remained stable and throughput did not degrade, so the codec core shows
  no early super-linear scaling signal.
- The production frame needs explicit little-endian metadata, stored raw sizes for bounded Zstd
  decode, per-frame checksums, and an EOS footer. It can reuse `ReadRecord` and does not need legacy
  `FQCWriter`/`FQCReader` types.
- The production sequential v2 archive and injected-stream engine now pass exact all-field,
  corruption, truncation, paired-atomicity, profile, and canonical FASTQ round-trip tests.
- `FastqParser::validateSequence` previously ignored `ParserOptions::validBases`; v2 now validates
  against the configured full upper/lower-case IUPAC nucleotide alphabet.

## Production fallback benchmark

Full release CLI path on an 8-core/16-thread AMD Ryzen 7 5800H x86_64 host, including FASTQ
parsing, frame construction, checksums, filesystem I/O, and exact round-trip comparison:

| Synthetic shape | Input | Compress MiB/s | Decompress MiB/s |
|---|---:|---:|---:|
| Illumina 150 bp | 65.49 MiB | 65.40 | 177.53 |
| Illumina 150 bp | 262.29 MiB | 76.72 | 256.76 |
| ONT-like 20 kbp | 256.02 MiB | 86.02 | 290.91 |

- The simple sequential production path clears the x86_64 50/100 release floor and scales
  linearly over the measured range, so adding a TBB graph now would add coordination risk without
  evidence of need. Keep frame boundaries as the future parallelism seam.
- These fixtures are deliberately repetitive and suitable for throughput/scaling, not claims about
  biological compression ratio. Randomised and real-corpus gates remain necessary.
- ARM64 remains unverified in the current environment and cannot be claimed as passing.

Randomised full-path evidence:

| Budget/shape | Compress MiB/s | Decompress MiB/s | Compress RSS | Decompress RSS |
|---|---:|---:|---:|---:|
| 512 MiB / Illumina 150 bp | 52.13 | 164.97 | 192.2 MiB | 128.5 MiB |
| 512 MiB / ONT-like 20 kbp | 58.62 | 209.65 | 163.3 MiB | 109.4 MiB |
| 64 MiB / Illumina 150 bp | 53.15 | 182.40 | 31.4 MiB | 12.1 MiB |
| 64 MiB / ONT-like 20 kbp | 55.66 | 215.22 | 25.5 MiB | 12.9 MiB |

The final 64 MiB rows are three-run medians after logical-record validation was moved into the
existing stream measurement loops. Their archive ratios were 2.9487x and 2.8401x respectively.
The first validation implementation scanned sequence and quality data redundantly and regressed
Illumina compression to 45.84 MiB/s; scan fusion restored the SLA without weakening validation.

## Current v2 shape

- Eight CTest targets replace the previous 16-target split architecture; all pass in under one
  second in the current debug build.
- The production build graph contains only common errors/logging/records, v2 format, FASTQ/gzip
  I/O, the injected stream factory, one archive engine, and the minimal CLI.
- oneTBB and hwloc are no longer dependencies. Legacy source, headers, tests, tracked v1 benchmark
  results, and duplicate CLI/benchmark smoke scripts were deleted.
- Compression and decode both perform conservative aggregate peak-memory preflight. Exceptional
  sequence symbols are emitted without a per-exception temporary object.
- File output is staged beside the destination and atomically published after success; failed
  paired input leaves no partial archive. stdout remains a direct stream.
- Compressed stdin now feeds zlib through a prefix-preserving streambuf instead of buffering the
  entire input.
- Gzip input accepts concatenated members, move assignment preserves unread inflate output, and the
  regression fixture now uses process-unique temporary directories so test presets can run in
  parallel.
- Installed CMake package metadata was validated with a separate consumer project; Conan now
  supplies zlib-ng in compatibility mode rather than silently resolving the system zlib.
