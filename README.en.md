# fq-compressor

A C++23 command-line FASTQ archiver.

fq-compressor losslessly compresses FASTQ sequencing reads into a compact FQC archive and restores them byte-for-byte. It keeps read IDs, comments, sequences, and quality strings; handles single-end and paired-end data; reads `.gz` input directly; and works through stdin/stdout pipelines.

## What it does

The tool focuses on one thing: sequential FASTQ archiving with exact recovery. It skips random access, global reordering, and lossy modes so it can offer steady throughput, bounded memory use, and simple failure boundaries.

The encoding is straightforward:

- ID/comment, sequence, and quality streams are handled separately;
- Uppercase A/C/G/T is packed to 2 bits; other IUPAC symbols and lowercase bases are stored as exact-position exceptions;
- Each stream is varint-framed and compressed with Zstd level 1;
- XXH64 checksums cover the global header, every frame, and the footer.

Profiles for `illumina`, `ont`, `pacbio-hifi`, and `pacbio-clr` can be auto-detected or set explicitly. All currently share the same validated codec; a specialised codec will only be added if it consistently improves size on real benchmark data without regressions.

## Usage

```bash
# Single-end; profile is auto-detected.
fqc compress -i reads.fastq -o reads.fqc

# Paired-end; each R1/R2 pair stays adjacent and frames never split a pair.
fqc compress -i reads_R1.fastq -2 reads_R2.fastq -o paired.fqc

# Explicit long-read profile.
fqc compress -i ont.fastq.gz -o ont.fqc --profile ont

# Verify and decompress.
fqc verify reads.fqc
fqc decompress -i reads.fqc -o restored.fastq

# Pipelines.
producer | fqc compress -i - -o - | consumer
fqc decompress -i reads.fqc -o - | downstream-tool
```

Global options may appear before or after the subcommand:

```text
--memory-limit MiB   Memory budget; default 16384, minimum 64
-q, --quiet          Suppress non-error output
```

Compression also accepts `--profile`, `--frame-mib`, and `--force`. Decompression accepts `--force`. Paired archives decompress as one canonical interleaved FASTQ stream.

## Build

Requirements: CMake 3.28+, Conan 2.x, GCC 14+ or Clang 18+.

```bash
conan profile detect --force
./scripts/build.sh clang-release
```

The executable is at `build/clang-release/src/fqc`.

## Archive format

The FQC archive consists of a checked global header, independent frames, and an end footer. The reader rejects unknown versions/codecs, oversized frames, inconsistent paired counts, truncation, trailing data after the footer, and checksum mismatches. See [`ARCHITECTURE.md`](ARCHITECTURE.md) for the byte layout.

## Benchmarks

End-to-end release CLI measurements on an 8-core AMD Ryzen 7 5800H x86_64 host:

| Synthetic input | Compress | Decompress | Max RSS at 64 MiB budget |
|---|---:|---:|---:|
| Randomised Illumina-like 150 bp | 53.15 MiB/s | 182.40 MiB/s | 31.4 MiB |
| Randomised ONT-like 20 kbp | 55.66 MiB/s | 215.22 MiB/s | 25.5 MiB |

These are pinned three-run medians from a WSL2 environment, including FASTQ parsing, checksums, frame construction, file I/O, decompression, and exact comparison. They are not claims about real biological compression ratios or stable guarantees across platforms. The current release is tested and published on x86_64: Linux glibc, static Linux musl, and macOS Intel.

Reproduce locally:

```bash
FQC_BIN=build/clang-release/src/fqc \
FQC_PERF_SIZES="64 256" \
FQC_PERF_DATA=random \
FQC_PERF_ENFORCE_SLA=1 \
bash tests/e2e/test_performance.sh
```

See [`performance/INDEX.md`](performance/INDEX.md) for the full performance record.

## Scope

The released product is only the `fqc` command-line executable. The headers under `include/fqc` and the `fqc_core`/`fqc_cli` CMake targets are internal module boundaries used by source builds and tests; they do **not** form a supported C++ API, ABI, or CMake package and may change without notice.

Current release artifacts cover x86_64: Linux glibc, static Linux musl, and macOS Intel.

## Development and verification

```bash
./scripts/test.sh clang-debug
./scripts/lint.sh format-check
```

Tests cover the FQC wire format, corruption/truncation, memory admission, parser/I/O, single/paired round trips, stdin/stdout, and the real CLI process boundary.

## License

MIT. See [`LICENSE`](LICENSE).
