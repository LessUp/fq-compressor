# FQC v2 architecture

FQC v2 is a sequential framed archive. The design optimises for bounded memory, process pipelines,
simple failure boundaries, and measured throughput. It deliberately does not provide v1
compatibility, an index, random access, global read reordering, or a second parallel engine.

The supported product boundary is the `fqc` CLI. Targets named `fqc_core` and `fqc_cli` and headers
under `include/fqc` organise the source tree and tests only; they are not installed and carry no
source or ABI compatibility promise.

## Data flow

```text
FASTQ/.gz/stdin
    -> FastqParser
    -> profile sample and resolution
    -> retained-byte frame accumulator
    -> three logical stream encoders
    -> checked FQC v2 frame
    -> file/stdout

FQC v2/file/stdin
    -> checked header
    -> bounded frame decode
    -> logical checksum
    -> canonical FASTQ writer
    -> file/stdout
```

The internal orchestration boundary is
`include/fqc/commands/v2_archive_engine.h`. The CLI in `src/main.cpp` only parses arguments,
constructs requests, and reports structured errors.

## Archive layout

All integers are little-endian. Sizes needed for allocation are stored before the corresponding
payload and are checked before memory is allocated.

| Region | Contents | Integrity/bounds |
|---|---|---|
| 32-byte global header | magic, version, flags, profile, stream codec IDs | XXH64 header checksum; strict version/codec IDs |
| Repeated 72-byte frame header | frame ID, record count, raw and encoded stream sizes | monotonic frame IDs; complete-pair invariant; per-field and aggregate memory limits |
| ID payload | record count, ID/comment lengths and bytes | Zstd level 1 |
| Sequence payload | record count, length, 2-bit A/C/G/T, exact exceptions | Zstd level 1 |
| Quality payload | record count, quality lengths and bytes | Zstd level 1 |
| 40-byte footer | frame/read/base totals and rolling checksum | totals must match decoded state |

The logical frame checksum covers the three uncompressed streams. The footer checksum advances
over each frame checksum, so reordering, omission, duplication, payload corruption, and inconsistent
totals are detected.

## Sequence representation

Uppercase A/C/G/T use two bits each. Every other accepted IUPAC symbol, including lowercase bases,
is stored as an exact byte plus a delta-coded position. Encoding makes one packing pass and only
makes a second positional pass when exceptions exist; it emits exceptions directly and does not
allocate an exception object per base. This matters for lowercase or ambiguity-heavy long reads,
where the legacy temporary vector could dwarf the input.

The v2 admission preflight validates the full upper/lower-case IUPAC alphabet while measuring the
same stream, avoiding a duplicate parser scan. Archive decode validates exception bytes and quality
scores before emitting a record. Sequence and quality lengths must match in both directions.

## Profiles

`auto` samples at most 50,000 records or 512 MiB of bases, further capped by one eighth of the
available operation budget.

- Illumina: short reads (maximum at most 1,000 bp and average at most 500 bp).
- ONT: majority header evidence such as `runid=` or channel tags.
- PacBio HiFi: majority `/ccs` or `hifi` header evidence.
- PacBio CLR: majority PacBio/subread or movie/ZMW/subread-style header evidence.

Ambiguous long-read input is rejected with an instruction to pass `--profile`; it is not silently
misclassified. Profiles are recorded in the archive but currently share fallback coding. This is
intentional: no specialised Illumina, HiFi, ONT, or CLR codec has yet passed the size and throughput
admission gate on an adequate real-data corpus.

## Memory model

The CLI default is 16 GiB and the supported minimum is 64 MiB. The codec uses no scratch files;
regular-file output is staged beside the destination and published only after the archive is
complete.

Compression applies three levels of control:

1. Profile sampling is capped by bytes and record count.
2. Frame accumulation charges the actual capacities of retained record strings and closes a frame
   at the smaller of the requested target and the budget-derived target.
3. Before encoding, the writer exactly measures all raw stream sizes, adds retained record
   capacities, Zstd output bounds, vector metadata, and a fixed codec/runtime reserve, then rejects
   a frame whose conservative peak exceeds the operation budget.

Decompression validates each individual raw/encoded size and also an aggregate conservative peak:
encoded bytes + two copies of raw logical bytes + decoded record metadata + runtime reserve. A
hostile header therefore cannot allocate three individually legal streams whose sum violates the
budget.

The max frame size is derived from `(memory limit - runtime reserve) / 4`; the accumulation target
uses `/ 8`. At a 64 MiB configured limit, measured release-process RSS was 25--32 MiB for randomised
short- and long-read fixtures.

## Execution architecture

The engine is intentionally sequential. Final three-run medians on the available 8-core x86_64
host reached 53--56 MiB/s compression and 182--215 MiB/s decompression on randomised data, already
clearing the 50/100 floor. Adding a TBB DAG without a demonstrated bottleneck would restore
duplicate state, output ordering, and in-flight-memory risks that v2 removed.

Independent frame boundaries remain the concurrency seam. If a release machine later misses the
floor, parallel work should be added as one ordered frame pipeline with byte-budget admission—not as
a separate compression implementation. Codec state must remain frame-local or worker-local.

## Paired reads

Two input files are read in lockstep and stored R1, R2, R1, R2. Different record counts are a hard
format error. A paired frame must contain an even number of records, so a pair never crosses a frame
boundary. Decompression produces one canonical interleaved FASTQ stream.

## Failure behaviour

- Output overwrite requires `--force`.
- Input and output paths may not be the same file.
- Truncation, unknown format/version/codec, checksum mismatch, impossible stream lengths, trailing
  logical-stream or post-footer bytes, pair-count disagreement, and memory-limit violations fail
  closed.
- `verify` performs the same full frame decode and checks as decompression but emits no FASTQ.
- v1 input is rejected; there is no partial or heuristic compatibility path.

## Modules

| Namespace | Responsibility |
|---|---|
| `fqc::format::v2` | wire format, stream coding, checksums, memory preflight |
| `fqc::commands::v2` | profile resolution and single bounded engine |
| `fqc::io` | FASTQ parsing, gzip input, file/stdin/stdout stream factories |
| `fqc::common` | records, structured errors, logging |

Legacy ABC, SCM, v1 format/index/reorder map, async-I/O island, paired parser, and TBB pipeline
modules were deleted rather than retained as unreachable compatibility code.
