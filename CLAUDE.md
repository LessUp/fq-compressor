# CLAUDE.md

> **See AGENTS.md for full project rules, build commands, and architecture.**

This repository is in **closeout mode**. Optimize for finishing, simplifying, and documenting.
Current work should follow the repository docs and scripts directly; OpenSpec is archival only.

## Quick Reference

- **Build**: `./scripts/build.sh clang-debug`
- **Test**: `./scripts/test.sh clang-debug`
- **Lint**: `./scripts/lint.sh format-check`
- **Push**: Direct push is default; no PR required

## Development Workflow

```
Understand → Implement → Verify → Commit
```

1. Read relevant code and docs first
2. Write code directly, following existing patterns
3. Run tests and lint checks
4. Push when checks pass

For risky changes, use `/review` before pushing.

See **AGENTS.md** for complete details.
