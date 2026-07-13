# Troubleshooting log

Record of problems found during development, testing, and use of `fq-compressor`: symptoms,
reproduction, root-cause analysis, fixes, and follow-up. One file per issue, numbered `NNNN-slug.md`.

## Layout

```text
troubleshooting/
  README.md     # this file
  INDEX.md      # status table: number | title | status | severity | date
  TEMPLATE.md   # copy this when opening a new issue
  NNNN-*.md     # one file per issue
```

## Conventions

- **Numbering**: zero-padded 4 digits, monotonically increasing. Never reuse a number.
- **Status**: `open` / `closed` / `wontfix`. Update `INDEX.md` when status changes.
- **Severity**: `critical` (data loss / security) / `high` (broken core feature) /
  `medium` (degraded but usable) / `low` (cosmetic / nuisance).
- **Slug**: short kebab-case, describes the symptom or root cause, not the fix.
- Link the related `performance/optimizations/NNNN-*.md` when an issue drove a perf change,
  and vice versa.

## Opening a new issue

```bash
cp troubleshooting/TEMPLATE.md troubleshooting/$(printf '%04d' $((10#$(ls troubleshooting/ | \
  grep -oE '^[0-9]{4}' | sort -n | tail -1) + 1)))-<slug>.md
```

Then add a row to `INDEX.md`.
