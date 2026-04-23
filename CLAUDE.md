# CLAUDE.md

This repository is in **closeout mode**. Optimize for finishing, simplifying, and documenting the
current version of `fq-compressor` rather than expanding scope.

## Core posture

- Treat `openspec/specs/` as the **only living requirements source**.
- Treat `specs/` as archived reference only.
- Prefer deleting or consolidating stale material over preserving duplicate explanations.
- Prefer small, focused direct pushes over large mixed cleanups.
- Avoid speculative features, large dependency additions, and wide workflow sprawl.

## Required workflow

1. Run `./scripts/dev/preflight.sh` before substantial work.
2. Check `openspec list --json` and work on **one active change at a time**.
3. Use `./scripts/dev/create-worktree.sh <change-name>` only when you want extra isolation.
4. Implement against OpenSpec tasks, not free-form prompts.
5. Run the smallest relevant validation first, then the repository checks:
   - `./scripts/lint.sh format-check`
   - `./scripts/test.sh clang-debug`
6. Use `/review` for risky or cross-file changes when it adds signal.
7. Push directly once local validation is green; a PR is not required for the default solo workflow.

## Tool roles

- **Claude / Codex / Copilot**: architecture edits, cross-file refactors, workflow/tooling updates.
- **GLM / lower-cost models**: bounded OpenSpec tasks with explicit acceptance criteria.
- **review models / subagents**: diff review, workflow review, doc drift review.

## LSP / MCP guidance

- Default to **clangd + compile_commands.json + CMake presets**.
- Keep MCP usage minimal. Prefer built-in GitHub tooling, `gh`, OpenSpec skills, and review flows.
- Do not add project-level MCP or plugin config unless it clearly reduces repeated manual work.

## Repository specifics

- Build with `./scripts/build.sh <preset>`.
- Test with `./scripts/test.sh <preset>`.
- Lint with `./scripts/lint.sh format-check` and `./scripts/lint.sh lint clang-debug`.
- Docs should stay **lean, bilingual, and non-duplicative**.
- GitHub Pages should act as a **project landing surface**, not a README mirror.
