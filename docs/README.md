# fq-compressor documentation

This repository keeps documentation intentionally **lean and bilingual**.

## Source of truth by surface

| Surface | Role |
| --- | --- |
| `README.md` | Repository entry point and quick orientation |
| `docs/website/` | GitHub Pages source and primary public landing surface |
| `docs/en/` | English documentation pages referenced by the site |
| `docs/zh/` | Simplified Chinese documentation pages referenced by the site |
| `openspec/` | Living specifications and planned work |
| `specs/` | Archived historical specs only |

## Documentation policy

- Prefer **one good explanation** over the same explanation repeated in multiple places.
- Keep English and Chinese onboarding paths aligned, but do not mirror every paragraph unless it
  provides real value.
- Use GitHub Pages to explain the project, show proof points, and direct users to the right docs.
- Archive stale or low-value material instead of expanding the active docs tree.

## Maintenance workflow

1. Change behavior or scope in `openspec/` first when requirements move.
2. Update the public-facing surface that actually owns the information:
   - README for repository entry copy
   - Pages for landing and navigation
   - `docs/en/` and `docs/zh/` for maintained reference pages
3. Remove redundant or outdated copies instead of preserving drift.
