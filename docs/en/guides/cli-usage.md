# CLI usage

This page keeps the operational command set compact. For implementation context, read [Architecture](/en/architecture/). For exact repository evidence, use [Resources](/en/resources/).

## Global options

Apply these before the subcommand:

- `-t, --threads <n>`: set worker count (`0` means auto)
- `-v, --verbose`: increase log verbosity
- `-q, --quiet`: suppress non-error output
- `--memory-limit <MB>`: cap memory budget
- `--no-progress`: disable progress display
- `--log-file <path>` / `--log-level <level>`: redirect or tune logging
- `--json`: JSON output for `info` and `verify`
- `-V, --version`: print version

## Compress

```bash
fqc compress -i reads.fastq -o reads.fqc
```

Useful options:

- `-2, --input2 <path>`: second FASTQ for paired-end input
- `--paired`: paired-end mode
- `--interleaved`: paired-end reads already interleaved in one file
- `-l, --level <1-9>`: compression level
- `--preserve-order` or `--no-reorder`: disable global reordering
- `--streaming`: stdin-friendly mode with lower compression potential
- `--quality-mode <lossless|illumina8|discard>`
- `--id-mode <exact|tokenize|discard>`
- `--long-read-mode <auto|short|medium|long>`
- `--block-reads <n>` / `--max-block-bases <n>`: block sizing controls
- `-f, --force`: overwrite an existing output file

Examples:

```bash
fqc compress -i reads.fastq.gz -o reads.fqc -t 8
fqc compress -i r1.fastq -2 r2.fastq -o paired.fqc --paired
fqc compress -i - -o stream.fqc --streaming
```

## Decompress

```bash
fqc decompress -i reads.fqc -o restored.fastq
```

Useful options:

- `--range <start:end>`: extract a read interval
- `--range-pairs <start:end>`: extract paired-end intervals in pair units
- `--original-order`: rebuild original order when a reorder map is present
- `--header-only`: output identifiers only
- `--streams <all|id|seq|qual|...>`: decode selected streams
- `--output-format <fastq|fasta|tsv|raw>`
- `--split-pe`: split paired-end output
- `--skip-corrupted`: continue past corrupted blocks
- `--verify` / `--no-verify`: control checksum verification during decode

Examples:

```bash
fqc decompress -i reads.fqc --range 1:1000 -o subset.fastq
fqc decompress -i paired.fqc -o restored.fastq --original-order
fqc decompress -i archive.fqc -o headers.txt --header-only
```

## Inspect and verify

```bash
fqc info -i reads.fqc
fqc verify -i reads.fqc
```

Useful options:

- `fqc info --json --detailed --show-index --show-codecs`
- `fqc verify --mode quick`
- `fqc verify --fail-fast --json`

## Operator pattern

```bash
fqc compress -i reads.fastq -o reads.fqc -t 8
fqc verify -i reads.fqc
fqc info -i reads.fqc --show-index
fqc decompress -i reads.fqc --range 1001:2000 -o spot-check.fastq
```

## Continue with

- [Getting started](./getting-started) for a first verified run
- [Contributing](./contributing) for repo checks and documentation updates
