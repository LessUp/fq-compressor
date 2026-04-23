## Why

The repository currently exposes multiple AI tool entrypoints without a single shared workflow. To
make closeout execution reliable, every tool needs the same OpenSpec-first process tuned for a
solo-maintainer, direct-push project.

## What Changes

- Publish a shared AI collaboration workflow centered on OpenSpec, local validation, and optional
  `/review`.
- Add targeted project instructions for Claude and Copilot.
- Reduce tool drift by defining what each model type should and should not own.

## Capabilities

### New Capabilities

- `ai-collaboration`: defines the required workflow and project-level instructions for AI-assisted
  development in this repository.

### Modified Capabilities

- `cli`: document how repository tooling should be used to inspect GitHub state before and after work.

## Impact

- `AGENTS.md`
- `CLAUDE.md`
- `.github/copilot-instructions.md`
- `.claude/`
- Tool-specific OpenSpec command/skill folders that remain project-level
