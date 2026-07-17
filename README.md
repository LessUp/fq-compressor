# fq-compressor

`fq-compressor` is a C++23 FASTQ compressor built around a small, sequential, bounded-memory
archive format. It preserves read IDs, comments, sequences, quality strings, and paired-read
adjacency. Plain FASTQ and `.gz` FASTQ inputs are supported.

> FQC v2 is intentionally incompatible with the legacy v1 archive and CLI. There is no v1 reader,
> migration command, random access, lossy mode, or original-order map.

## Why v2

The previous ABC/SCM implementation combined an expensive global analysis path, dense per-thread
quality models, and two separate serial/parallel engines. Its tracked throughput was roughly
0.1 MiB/s. V2 replaces that stack with independent sequential frames and one engine shared by
compression, decompression, and verification.

The production fallback uses:

- varint-framed ID/comment streams compressed with Zstd level 1;
- 2-bit uppercase A/C/G/T sequence packing plus exact-position exceptions for every other IUPAC
  symbol and lowercase base, followed by Zstd level 1;
- varint-framed quality streams compressed with Zstd level 1;
- XXH64 checksums for the global header, each logical frame, and footer totals.

Dataset profiles (`illumina`, `ont`, `pacbio-hifi`, and `pacbio-clr`) are recorded explicitly and
can be detected or overridden. They currently share the validated fallback codec. A specialised
codec is not admitted unless it keeps the throughput floor and beats fallback size by at least 5%
on the benchmark corpus without a per-dataset regression above 1%.

## Build

Requirements: CMake 3.28+, Conan 2.x, GCC 14+ or Clang 18+.

```bash
conan profile detect --force
./scripts/build.sh clang-release
```

The executable is `build/clang-release/src/fqc`.

## Support boundary

The released product is the `fqc` command-line executable. Install and Conan packages contain
`bin/fqc` only. The headers under `include/fqc` and the `fqc_core`/`fqc_cli` CMake targets are
internal module boundaries used by source builds and tests; they do not form a supported C++ API,
ABI, or CMake package and may change without compatibility guarantees.

## CLI

```bash
# Single-end; profile is auto-detected.
fqc compress -i reads.fastq -o reads.fqc

# Paired files. Each R1/R2 pair stays adjacent and each frame contains complete pairs.
fqc compress -i reads_R1.fastq -2 reads_R2.fastq -o paired.fqc

# Explicit long-read profile.
fqc compress -i ont.fastq.gz -o ont.fqc --profile ont

# Full integrity verification and decompression.
fqc verify reads.fqc
fqc decompress -i reads.fqc -o restored.fastq

# Process pipelines.
producer | fqc compress -i - -o - | consumer
fqc decompress -i reads.fqc -o - | downstream-tool
```

Global options may appear before or after the subcommand:

```text
--memory-limit MiB   Hard operation budget; default 16384, minimum 64
-q, --quiet          Suppress non-error status messages
```

Compression additionally accepts `--profile`, `--frame-mib`, and `--force`. Decompression accepts
`--force`. Paired archives decompress as one canonical interleaved FASTQ stream.

## Format and compatibility

FQC v2 consists of a checked global header, zero or more independent frames, and an end footer.
The reader rejects unknown versions/codecs, oversized frames, inconsistent paired counts,
truncation, trailing logical-stream or post-footer data, and checksum mismatches. It does not probe
or decode v1.

See [ARCHITECTURE.md](ARCHITECTURE.md) for the byte layout and memory model.

## Measured performance

Release CLI measurements on an 8-core AMD Ryzen 7 5800H x86_64 host include FASTQ parsing,
checksums, frame construction, file I/O, decompression, and exact comparison.

| Synthetic input | Compress | Decompress | Max RSS at 64 MiB budget |
|---|---:|---:|---:|
| Randomised Illumina-like 150 bp | 53.15 MiB/s | 182.40 MiB/s | 31.4 MiB |
| Randomised ONT-like 20 kbp | 55.66 MiB/s | 215.22 MiB/s | 25.5 MiB |

These are the pinned three-run medians from the WSL2 environment recorded in
[performance/INDEX.md](performance/INDEX.md), not biological compression-ratio claims or stable
release-machine guarantees. WSL2 wall-clock noise has produced lower reruns, including Illumina
compression below 50 MiB/s. The repository does not currently contain enough real ONT/HiFi/CLR
data to admit a specialised long-read codec, and ARM64 remains an explicit release-machine
verification item. The table includes full logical-record validation and the maximum RSS across
repetitions.

Run the local scaling harness with:

```bash
FQC_BIN=build/clang-release/src/fqc \
FQC_PERF_SIZES="64 256" \
FQC_PERF_DATA=random \
FQC_PERF_ENFORCE_SLA=1 \
bash tests/e2e/test_performance.sh
```

`tests/e2e/test_performance.sh` is the throughput/RSS/SLA harness. The user-facing dataset-driven
peer comparison, including Spring when configured, is `./scripts/benchmark.sh`.
Codec pass/fail decisions are recorded in [benchmark_v2/CODEC_GATES.md](benchmark_v2/CODEC_GATES.md).

## Development

The project is in closeout mode. Keep changes small, delete duplicate paths, and run:

```bash
./scripts/test.sh clang-debug
./scripts/lint.sh format-check
```

Focused tests cover the v2 wire format, corruption/truncation, memory admission, parser/I/O,
single/paired round trips, stdin/stdout, and the real CLI process boundary.

## License

MIT. See [LICENSE](LICENSE).
