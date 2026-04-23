# Copilot instructions for fq-compressor

- This project is **OpenSpec-driven**. Use `openspec/specs/` as the living requirements source and
  treat `specs/` as archived reference only.
- The repository is in **closeout mode**: prefer finishing, simplifying, consolidating, and fixing
  over adding new surface area.
- Before substantial work, check:
  - `./scripts/dev/preflight.sh`
  - `openspec list --json`
- Use **one change at a time** and keep work aligned with `proposal.md`, `design.md`, and `tasks.md`.
- Direct push is the default solo-maintainer workflow. `git worktree` is optional and only needed
  when you want extra isolation.
- Use GitHub-native commands intentionally:
  - `/research` for bounded repository investigation before a change proposal
  - `/review` for risky or wide diffs before pushing
  - `/remote` only when the remote execution is tied to a specific OpenSpec change
- Use `./scripts/build.sh`, `./scripts/test.sh`, and `./scripts/lint.sh`; do not invent parallel
  build/test entrypoints when existing scripts already cover the repository.
- Prefer `clangd + compile_commands.json + CMake presets` for navigation and diagnostics.
- Keep docs bilingual but lean. Remove duplication rather than creating another layer of summary
  files.
- Keep GitHub Actions and engineering config minimal; avoid high-cost, low-signal workflow changes.
