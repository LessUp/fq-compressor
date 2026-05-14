# fq-compressor documentation

This repository keeps documentation intentionally **lean and focused**.

## Authoritative surfaces

| Surface | Role |
| --- | --- |
| `openspec/specs/` | Living requirements and capability specifications |
| `openspec/changes/` | Active change work and closeout planning |
| `README.md` / `README.zh-CN.md` | Repository entry summary |
| `docs/` | GitHub Pages source (VitePress public docs package) |
| `docs/benchmark/` | Benchmark evidence and methodology |

## Archived surfaces

| Surface | Status |
| --- | --- |
| `docs/archive/` | Archive-only - historical research, reviews, and specs |
| `docs/dev/` | Development notes |

## Documentation policy

- **Single source of truth**: `docs/` is the only public documentation source
- **Public docs stack**: VitePress sources live directly under `docs/`
- **Bilingual**: Public docs support both English (`/en/`) and Chinese (`/zh/`) routes
- **No duplication**: Remove stale duplicates instead of keeping them "just in case"
- **Evidence-based**: Benchmark claims must link to artifacts in `docs/benchmark/` or `benchmark/results/`
