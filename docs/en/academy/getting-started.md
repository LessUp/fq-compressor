# Getting started

Use this page when you already have `fqc` available and want the shortest path to a verified `.fqc` round trip.
If you still need the binary, start with [Installation](./installation). For the command surface, continue with
[CLI workflows](./cli-workflows). For source and benchmark links, keep [Research](/en/research/) nearby.

## 1. Confirm the binary

```bash
build/clang-debug/src/fqc --version
build/clang-debug/src/fqc --help
```

## 2. Run a smoke-scale round trip

```bash
cat > sample.fastq <<'FASTQ'
@read_1
ACGTACGT
+
IIIIIIII
@read_2
TGCATGCA
+
JJJJJJJJ
FASTQ

build/clang-debug/src/fqc compress -i sample.fastq -o sample.fqc
build/clang-debug/src/fqc verify -i sample.fqc
build/clang-debug/src/fqc decompress -i sample.fqc -o restored.fastq
cmp sample.fastq restored.fastq
```

No output from `cmp` means the restored FASTQ matches the input.

## 3. Run the normal repository checks

```bash
./scripts/test.sh clang-debug
./scripts/lint.sh format-check
```

## Continue with

- [CLI workflows](./cli-workflows) for command patterns and key options
- [Installation](./installation) for release and source-build entry paths
- [Contributing](./contributing) for closeout-mode workflow and quality gates
- [Comparative study](/en/research/open-source-comparative-study) for related work
