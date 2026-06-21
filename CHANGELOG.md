# Changelog

This changelog is intentionally **conservative**. During closeout cleanup, low-confidence narrative
entries were removed in favor of a smaller ledger of changes that are easy to verify.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project
uses [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Removed

- Deleted `vendor/spring-core/` (unlinked Spring ABC extraction) and its references in
  `CMakeLists.txt`, `README.md`, `README.zh-CN.md`, `AGENTS.md`, and `docs/{en,zh}/research/*`.
  Spring remains acknowledged as an algorithm reference only.
- Removed the `memory_budget` module (`include/fqc/common/memory_budget.h`,
  `src/common/memory_budget.cpp`, `tests/common/memory_budget_test.cpp`) and its CMake
  registrations. The module had no production callers and existed only as a test island.
- Removed the unused `IPipelineStage` template interface from `include/fqc/pipeline/pipeline.h`;
  the TBB pipeline uses direct lambdas and never instantiated this interface.

### Changed

- Removed stale OpenSpec workflow guidance from repository docs and archive surfaces.
- Consolidated changelog expectations around a single English `CHANGELOG.md` and deleted the
  Chinese duplicate.
- Demoted `pipeline_node.h` and `pipeline_nodes.h` to documented legacy compatibility includes;
  `pipeline.h` remains the preferred integration surface.

## [0.1.0] - 2026-04-16

### Added

- Initial public C++ implementation of `fq-compressor`
- Core `compress`, `decompress`, `info`, and `verify` command set
- FASTQ compression/decompression pipeline, FQC archive format, and random-access foundations
- Google Test-based unit coverage, GitHub Actions, and bilingual project documentation

## Notes

- Historical pre-release narrative detail was intentionally removed because it was not reliable
  enough to keep as the canonical changelog.
- Future entries should remain short, release-oriented, and evidence-backed.

[Unreleased]: https://github.com/LessUp/fq-compressor/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/LessUp/fq-compressor/releases/tag/v0.1.0
