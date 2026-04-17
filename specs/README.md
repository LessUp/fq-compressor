# fq-compressor Specifications

This directory contains the **authoritative specification documents** for fq-compressor, following **Spec-Driven Development (SDD)** methodology.

## Directory Structure

```
specs/
├── product/    # Product Requirements Documents (PRD)
├── rfc/        # Technical Design & Architecture (RFCs)
└── testing/    # Test specifications (BDD, acceptance criteria)
```

## Document Index

### Product Specifications (`/specs/product/`)

| Document | Purpose |
|----------|---------|
| [requirements.md](product/requirements.md) | Functional requirements, CLI design, non-functional requirements |
| [tasks.md](product/tasks.md) | Implementation plan with phases and checkpoints |

### Technical Design (`/specs/rfc/`)

| Document | Purpose |
|----------|---------|
| [0001-architecture.md](rfc/0001-architecture.md) | System architecture, file format, component design |
| [0002-compression-algorithms.md](rfc/0002-compression-algorithms.md) | ABC (Sequence), SCM (Quality), Delta+Token (ID) algorithms |
| [0003-parallel-pipeline.md](rfc/0003-parallel-pipeline.md) | TBB pipeline, memory management, async I/O |
| [0004-long-read-support.md](rfc/0004-long-read-support.md) | Long read classification, compression strategies |
| [0005-paired-end-support.md](rfc/0005-paired-end-support.md) | PE layouts, compression optimizations |

### Testing Specifications (`/specs/testing/`)

| Document | Purpose |
|----------|---------|
| [boundary-conditions.md](testing/boundary-conditions.md) | Edge cases, error handling, validation rules |
| [acceptance-criteria.md](testing/acceptance-criteria.md) | Feature acceptance criteria for verification |

## Workflow

Per SDD methodology, all code changes must:

1. **Reference existing spec** - Code implements what's documented here
2. **Update spec first** - Changes to behavior require spec updates before code
3. **Verify against spec** - Tests validate acceptance criteria defined in specs

See [AGENTS.md](../AGENTS.md) for AI assistant workflow instructions.
