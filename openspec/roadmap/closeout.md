# fq-compressor closeout roadmap

## Capability state

| Capability | State | Reason |
|------------|-------|--------|
| random-access | strengthen | Close gap between documented and main-path behavior |
| compression | stabilize | Core algorithm is production-ready |
| decompression | stabilize | Core algorithm is production-ready |
| file-format | stabilize | Binary format is stable |
| performance | strengthen | Benchmark claims need reproducibility |
| integrity | stabilize | Checksum and verification are complete |
| cli | stabilize | Command surface is stable |
| build-system | stabilize | CMake and Conan are production-ready |
| compatibility | stabilize | Version policy is defined |
| atomic-write | closeout | Production-ready, maintenance mode |
| quality-handling | invest | Active development for lossy modes |
| docs-site | strengthen | Improve onboarding and positioning |
| release-engineering | strengthen | Make release artifacts and support boundaries explicit |

## Roadmap phases

### Phase 1: Core path closeout

Close gap between documented and main-path random-access behavior; align reorder-map/original-order claims; keep file-format expectations explicit.

**Exit criteria:**
- Random-access behavior matches documentation
- Reorder-map and original-order semantics are clear
- File-format guarantees are explicit in capability specs

### Phase 2: Benchmark hardening

Use public datasets; compare against FASTQ-specific peers where practical; make public benchmark claims reproducible.

**Exit criteria:**
- Benchmarks use public datasets
- Results are reproducible in CI
- Comparisons with peers are fair and documented

### Phase 3: Release readiness

Make release artifacts and support boundaries explicit; align README, docs site, and release surfaces.

**Exit criteria:**
- Release artifacts are well-defined
- Support boundaries are clear
- Documentation surfaces are consistent

### Phase 4: Closeout readiness

Reduce maintenance burden; keep only a narrow high-signal future queue.

**Exit criteria:**
- Documentation is complete and consolidated
- Legacy material is archived
- Handoff preparation is complete

## First-wave changes

1. `random-access-closeout`
2. `benchmark-hardening`
3. `release-readiness`
4. `docs-site-consolidation`
