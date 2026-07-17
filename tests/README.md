# fq-compressor tests

This directory contains focused unit and end-to-end tests for FQC v2.

## Testing frameworks

- **Google Test (GTest)** for unit tests

## Test file naming convention

- Unit tests: `*_test.cpp`

## Directory structure

- `commands/` — Bounded v2 engine tests
- `common/` — Structured error tests
- `e2e/` — End-to-end CLI tests (shell scripts)
- `format/` — FQC v2 wire-format, integrity, and memory-bound tests
- `io/` — FASTQ parser and stream tests

## Running tests

```bash
# Run all tests with a preset
./scripts/test.sh clang-debug

# Run with filter
./scripts/test.sh clang-debug 'V2Archive*'

# Run via ctest directly
ctest --test-dir build/clang-debug --output-on-failure
```

## Release acceptance

Use the single acceptance runner for the validated release loop:

```bash
./scripts/acceptance.sh
```

Blocking release-check command surface (kept in sync with the acceptance runner):

```bash
./scripts/lint.sh format-check
./scripts/test.sh clang-debug
bash tests/e2e/cli_smoke_test.sh
bash tests/e2e/benchmark_v2_smoke_test.sh
```

Devcontainer validation is a separate, non-blocking CI concern because live validation requires
Docker and npx. Run its contract and live configuration checks explicitly on a prepared host:

```bash
bash tests/e2e/devcontainer_validate_test.sh
bash tests/e2e/devcontainer_host_sync_test.sh
bash tests/e2e/devcontainer_sshd_lib_test.sh
bash tests/e2e/devcontainer_adapter_contract_test.sh
bash scripts/devcontainer-validate.sh
```

Benchmark smoke is a local validation path, not a public performance claim.
