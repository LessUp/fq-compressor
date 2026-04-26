## 1. Add reusable workflow helpers

- [ ] 1.1 Add a preflight script for local repo state, dirty tree state, and active OpenSpec changes
- [ ] 1.2 Keep a change-oriented worktree bootstrap script as an optional helper

## 2. Wire helpers into the engineering flow

- [ ] 2.1 Add a pre-push/manual hook entry that runs the preflight script in strict mode
- [ ] 2.2 Update project scripts and workflows to prefer deterministic installs over mutable ones

## 3. Standardize editor support

- [ ] 3.1 Publish a root `.clangd` config aligned with `compile_commands.json`
- [ ] 3.2 Verify the resulting workflow still uses existing build/test/lint entrypoints
