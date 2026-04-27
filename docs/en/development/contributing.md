---
title: Contributing
description: Closeout-oriented contribution workflow for fq-compressor
version: 0.2.0
last_updated: 2026-04-27
language: en
---

# Contributing to fq-compressor

`fq-compressor` is in **closeout mode**. Contributions should focus on finishing, simplifying,
stabilizing, and documenting the current version rather than expanding product scope.

## Repository posture

- **OpenSpec-driven**: use `openspec/specs/` as the only living requirements source
- **One active change at a time**: align work to the current OpenSpec change instead of starting
  parallel cleanup tracks
- **Lean documentation**: prefer one authoritative explanation over multiple mirrors
- **Minimal tooling**: reduce maintenance surface when a simpler workflow is sufficient

## Source-of-truth map

Use the following surfaces intentionally:

| Surface | Role |
| --- | --- |
| `openspec/specs/` | Living requirements |
| `openspec/changes/` | Active change work and execution context |
| `README.md` / `README.zh-CN.md` | Repository entry summary |
| `docs/website/` | GitHub Pages source |
| `docs/en/` / `docs/zh/` | Maintained reference documentation |
| `specs/` | Archived reference only |
| `docs/archive/` | Historical reference only |

If you are about to edit `specs/` or `docs/archive/`, stop first and confirm that the material
really belongs in an archive surface.

## Default closeout workflow

1. Run the repository preflight:
   ```bash
   ./scripts/dev/preflight.sh
   openspec list --json
   ```
2. Work against the active OpenSpec change rather than free-form prompts.
3. Prefer targeted edits and targeted checks before running the default gate.
4. Keep the default local gate green:
   ```bash
   ./scripts/lint.sh format-check
   ./scripts/test.sh clang-debug
   ```
5. Use `/review` when the diff is risky, architectural, or unusually broad.
6. Push directly once the relevant checks are green. A pull request is optional, not the default
   solo-maintainer path.

## Optional isolation

Direct work on the current branch is acceptable for the default workflow. If you want extra
isolation, create a worktree from an OpenSpec change name:

```bash
./scripts/dev/create-worktree.sh <change-name>
```

Use this only when it reduces risk. It is not mandatory.

## Build, test, and lint

Prefer the repository scripts over ad-hoc command sequences:

```bash
./scripts/build.sh clang-debug
./scripts/test.sh clang-debug
./scripts/lint.sh format-check
./scripts/lint.sh lint clang-debug
```

For C++ editor support, the repository baseline is:

- `clangd`
- `compile_commands.json`
- CMake presets
- root `.clangd`

Do not introduce a second primary project-level C++ language-server stack unless there is a clear,
documented need.

## What kind of contributions fit this stage

Good closeout contributions:

- removing duplicated or stale documentation
- tightening README / Pages / docs ownership boundaries
- revalidating and fixing real defects with focused regression coverage
- simplifying CI, release, and Pages workflows
- consolidating AI guidance and repository tooling

Out-of-scope or high-bar contributions:

- adding major new product features
- expanding CI matrices without strong evidence
- introducing broad new toolchains or governance layers
- keeping duplicate docs “just in case”

## Documentation ownership

Update the surface that actually owns the information:

- repository summary -> `README*.md`
- public landing story -> `docs/website/`
- maintained reference pages -> `docs/en/` and `docs/zh/`
- requirements or workflow rules -> `openspec/`

Prefer deletion, archival, or consolidation over copying the same explanation into another file.

## Review and completion

Before finishing a non-trivial change:

1. Re-run the relevant checks.
2. Confirm the change still follows the active OpenSpec tasks.
3. Use `/review` if the resulting diff is risky or cross-cutting.
4. Keep the repository in a state that a later low-context maintainer or model can follow.

## License

By contributing, you agree that your contributions will be licensed under the MIT License.
