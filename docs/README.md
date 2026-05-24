# fq-compressor documentation

This repository keeps documentation intentionally **lean and focused**.

## Authoritative surfaces

| Surface | Role |
| --- | --- |
| `AGENTS.md` / `CLAUDE.md` | Repository workflow and closeout guidance |
| `README.md` / `README.zh-CN.md` | Repository entry summary |
| `docs/` | GitHub Pages source (VitePress public docs package) |
| `docs/benchmark/` | Benchmark evidence and methodology |

## Supporting surfaces

| Surface | Status |
| --- | --- |
| `docs/archive/` | Archive-only - historical research, reviews, and retired specs |
| `docs/dev/` | Development notes and helper scripts |
| `docs/superpowers/` | In-repo planning and specs support material |

## Documentation policy

- **Single source of truth**: repository docs plus code are the active source of truth
- **Public docs stack**: VitePress sources live directly under `docs/`
- **Bilingual**: Public docs support both English (`/en/`) and Chinese (`/zh/`) routes
- **No duplication**: Remove stale duplicates instead of keeping them "just in case"
- **Evidence-based**: Benchmark claims must link to artifacts in `docs/benchmark/` or `benchmark/results/`
