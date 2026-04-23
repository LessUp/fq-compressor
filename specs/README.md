# fq-compressor Specifications

This directory contains archived specification documents. **For active development, see the [openspec/](../openspec/) directory.**

## Migration Note

As of 2026-04-23, the project has migrated to [OpenSpec](https://github.com/Fission-AI/OpenSpec) for spec-driven development.

- **Living specifications**: `openspec/specs/`
- **Change proposals**: `openspec/changes/`
- **Project context**: `openspec/config.yaml`

## Archived Documents

The following documents are kept for historical reference:

### Product Specifications (`specs/product/`)

| Document | Status |
|----------|--------|
| [requirements.md](product/requirements.md) | Archived - migrated to OpenSpec |
| [tasks.md](product/tasks.md) | Completed - archived |

### Technical Design (`specs/rfc/`)

| Document | Status |
|----------|--------|
| [0001-architecture.md](rfc/0001-architecture.md) | Archived - migrated to `file-format` spec |
| [0002-compression-algorithms.md](rfc/0002-compression-algorithms.md) | Archived - migrated to `compression` spec |
| [0003-parallel-pipeline.md](rfc/0003-parallel-pipeline.md) | Archived - migrated to `performance` spec |
| [0004-long-read-support.md](rfc/0004-long-read-support.md) | Archived - migrated to `compression` spec |
| [0005-paired-end-support.md](rfc/0005-paired-end-support.md) | Archived - migrated to `compression` spec |

### Testing Specifications (`specs/testing/`)

| Document | Status |
|----------|--------|
| [boundary-conditions.md](testing/boundary-conditions.md) | Archived - scenarios in OpenSpec specs |
| [acceptance-criteria.md](testing/acceptance-criteria.md) | Archived - scenarios in OpenSpec specs |

## OpenSpec Workflow

For new development, use OpenSpec commands:

- `/opsx:propose "<idea>"` - Start a new change proposal
- `/opsx:apply <change>` - Implement the change
- `/opsx:archive <change>` - Complete and archive

See [AGENTS.md](../AGENTS.md) for full workflow details.
