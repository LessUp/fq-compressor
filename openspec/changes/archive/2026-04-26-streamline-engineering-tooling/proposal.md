## Why

The repository currently carries more engineering process weight than a closeout-stage project needs.
The remaining tooling should focus on high-signal validation and predictable developer setup instead
of expensive or noisy workflow churn.

## What Changes

- Add a single preflight script for local repository/OpenSpec checks.
- Keep worktree creation available only as an optional helper for isolated edits.
- Simplify selected workflow and tooling defaults around reproducibility and clangd.

## Capabilities

### New Capabilities

- `developer-tooling`: defines the required local workflow helpers, hooks, and editor/LSP baseline.

### Modified Capabilities

- `build-system`: adjust workflow expectations to favor deterministic, lockfile-respecting tooling.

## Impact

- `scripts/dev/`
- `.pre-commit-config.yaml`
- `.github/workflows/`
- `.vscode/`
- `.clangd`
- Root `package.json`
