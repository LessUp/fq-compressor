# fq-compressor documentation

This repository keeps documentation intentionally **lean and focused**.

## Authoritative surfaces

| Surface | Role |
| --- | --- |
| Archived spec snapshots | Historical requirements and capability notes |
| Archived change records | Historical closeout notes |
| `README.md` / `README.zh-CN.md` | Repository entry summary |
| `docs/` | GitHub Pages source (VitePress public docs package) |
| `docs/benchmark/` | Benchmark evidence and methodology |

## Archived surfaces

| Surface | Status |
| --- | --- |
| `docs/archive/` | Archive-only - historical research, reviews, and specs |
| `docs/dev/` | Development notes |
| `docs/superpowers/` | Internal/historical agent-workflow material; not project truth |

## Documentation policy

- **Single source of truth**: `docs/` is the only public documentation source
- **Public docs stack**: VitePress sources live directly under `docs/`
- **Bilingual**: Public docs support both English (`/en/`) and Chinese (`/zh/`) routes
- **No duplication**: Remove stale duplicates instead of keeping them "just in case"
- **Evidence-based**: Benchmark claims must link to artifacts in `docs/benchmark/` or `benchmark/results/`
