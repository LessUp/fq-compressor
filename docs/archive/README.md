# Archive Directory

This directory contains **historical reference material only**. These files are not the source of
truth for current development, closeout work, or repository governance.

## Use these surfaces instead

- `openspec/specs/` — living requirements
- `openspec/changes/` — active change work
- `README.md` / `README.zh-CN.md` — repository entry copy
- `docs/website/` — GitHub Pages source (bilingual)

## Directory Structure

```
docs/archive/
├── README.md              # This file
├── specs/                 # Archived specifications (migrated from root specs/)
│   ├── product/           # Product specs (archived)
│   ├── rfc/               # RFC specs (archived)
│   └── testing/           # Testing specs (archived)
├── reviews/               # Historical code reviews
│   └── 2026-01-*.md       # January 2026 review notes
├── design.md              # Original design document
├── requirements.md        # Original requirements
├── tasks.md               # Original task list
├── comprehensive-diagnosis.md  # Diagnostic notes
├── boundary-conditions.md # Boundary condition analysis
├── 0x-*.md                # Feasibility/strategy/algorithm analyses
└── references.md          # Reference links
```

## What belongs here

- Superseded research, planning notes, and historical reviews
- Legacy release notes or migration notes kept only for reference
- Diagnostic material that may still be useful for retrospective bug revalidation

If a document here becomes actively maintained again, it should move back to an authoritative surface
instead of being edited in place.
