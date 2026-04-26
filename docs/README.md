# fq-compressor documentation

This repository keeps documentation intentionally **lean, layered, and bilingual where it matters**.

## Authoritative surfaces

| Surface | Role | Edit when |
| --- | --- | --- |
| `openspec/` | Living requirements, change intent, closeout planning | Product or workflow requirements change |
| `README.md` / `README.zh-CN.md` | Repository entry summary | The top-level project story or quick-start pointers change |
| `docs/website/` | GitHub Pages source and public landing experience | The landing page, site IA, or public navigation changes |
| `docs/en/` / `docs/zh/` | Maintained reference pages used by Pages | A maintained reference page needs updating |

## Archived and generated surfaces

| Surface | Status | Notes |
| --- | --- | --- |
| `specs/` | Archive-only | Historical specs; do not treat as living requirements |
| `docs/archive/` | Archive-only | Historical notes, research, and superseded docs |
| `docs/.site/` | Generated output | Build artifact for Pages; never edit as source |

## Documentation policy

- Prefer **one good explanation** over multiple competing copies.
- Keep English and Chinese onboarding paths aligned, but do not mirror long-form text paragraph by
  paragraph without clear value.
- Use GitHub Pages as the public landing surface; use README as the repository entry surface.
- Remove, archive, or merge low-value documents instead of preserving drift.
- Treat migration notes, release notes, and historical diagnosis documents as disposable unless they
  still provide reference value that is not already captured elsewhere.

## Maintenance workflow

1. Change requirements or workflow policy in `openspec/` first.
2. Update only the surface that owns the affected information:
   - README for repository entry copy
   - Pages for landing and navigation
   - `docs/en/` / `docs/zh/` for maintained reference content
3. Archive or delete stale duplicates instead of keeping them "just in case".
