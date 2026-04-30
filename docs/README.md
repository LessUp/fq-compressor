# fq-compressor documentation

This repository keeps documentation intentionally **lean and focused**.

## Authoritative surfaces

| Surface | Role |
| --- | --- |
| `openspec/` | Living requirements, change intent, closeout planning |
| `README.md` / `README.zh-CN.md` | Repository entry summary |
| `docs/website/` | GitHub Pages source (Next.js + Nextra) |
| `docs/benchmark/` | Benchmark evidence and methodology |

## Archived surfaces

| Surface | Status |
| --- | --- |
| `specs/` | Archive-only - historical specs |
| `docs/archive/` | Archive-only - historical research |
| `docs/dev/` | Development notes |

## Documentation policy

- **Single source of truth**: `docs/website/` is the only public documentation source
- **Bilingual**: Website supports both English (`/en/`) and Chinese (`/zh/`) routes
- **No duplication**: Remove stale duplicates instead of keeping them "just in case"
- **Evidence-based**: Benchmark claims must link to artifacts in `docs/benchmark/` or `benchmark/results/`
