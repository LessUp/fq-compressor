## Why

Current `openspec/specs/*` lacks a project-level layer describing what to strengthen, what to
stabilize, and which changes belong in the near-term closeout queue. Recent FASTQ compressor
research shows the release path should focus on random access, reproducible benchmarking, release
readiness, and documentation clarity rather than broad feature expansion.

## What Changes

- Add one active OpenSpec change for a project-level closeout roadmap.
- Add `openspec/roadmap/closeout.md`.
- Add `docs-site` and `release-engineering` capability specs.
- Add requirement deltas for `random-access`, `performance`, `compatibility`, and `build-system`.
- Keep the roadmap closeout-oriented.

## Capabilities

### New Capabilities

- `docs-site`: governs the documentation website structure, build, and deployment.
- `release-engineering`: governs release artifact generation, distribution, and verification.

### Modified Capabilities

- `random-access`: add closeout-phase requirements for reproducibility and performance validation.
- `performance`: add closeout-phase requirements for benchmark reproducibility and CI integration.
- `compatibility`: add closeout-phase requirements for version stability and migration guidance.
- `build-system`: add closeout-phase requirements for reproducible builds and release artifact
  generation.

## Impact

- `openspec/changes/introduce-closeout-roadmap/`
- `openspec/roadmap/closeout.md`
- `openspec/specs/docs-site.yaml`
- `openspec/specs/release-engineering.yaml`
- `openspec/specs/random-access.yaml`
- `openspec/specs/performance.yaml`
- `openspec/specs/compatibility.yaml`
- `openspec/specs/build-system.yaml`
- Contributor workflow anchored by `./scripts/dev/preflight.sh` and `openspec list --json`
