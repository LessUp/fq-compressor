# fq-compressor Specifications

This directory is **archive-only**. Do not use it as the source of truth for active development.

## Active sources of truth

- **Living specifications:** `openspec/specs/`
- **Active change work:** `openspec/changes/`
- **Project context:** `openspec/config.yaml`

If you are planning or implementing current work, stop reading here and switch to `openspec/`.

## Archived documents kept for reference

### Product specifications (`specs/product/`)

| Document | Status |
|----------|--------|
| [requirements.md](product/requirements.md) | Archived - migrated to OpenSpec |
| [tasks.md](product/tasks.md) | Archived - superseded by OpenSpec change tasks |

### Technical design (`specs/rfc/`)

| Document | Status |
|----------|--------|
| [0001-architecture.md](rfc/0001-architecture.md) | Archived - migrated to `file-format` spec |
| [0002-compression-algorithms.md](rfc/0002-compression-algorithms.md) | Archived - migrated to `compression` spec |
| [0003-parallel-pipeline.md](rfc/0003-parallel-pipeline.md) | Archived - migrated to `performance` spec |
| [0004-long-read-support.md](rfc/0004-long-read-support.md) | Archived - migrated to `compression` spec |
| [0005-paired-end-support.md](rfc/0005-paired-end-support.md) | Archived - migrated to `compression` spec |

### Testing specifications (`specs/testing/`)

| Document | Status |
|----------|--------|
| [boundary-conditions.md](testing/boundary-conditions.md) | Archived - scenarios migrated into OpenSpec |
| [acceptance-criteria.md](testing/acceptance-criteria.md) | Archived - scenarios migrated into OpenSpec |

## Workflow note

Use OpenSpec for all new work:

- `/opsx:propose "<idea>"`
- `/opsx:apply <change>`
- `/opsx:archive <change>`

See [AGENTS.md](../AGENTS.md) for repository workflow details.
