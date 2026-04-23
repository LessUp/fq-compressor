## Context

Closeout work should not depend on contributors remembering a long list of manual checks. The
repository already has good build and test scripts, but it benefits from a single local-status check
and an optional helper for isolated worktrees.

## Goals / Non-Goals

**Goals:**
- Turn local repository/OpenSpec checks into one reusable command.
- Keep worktree bootstrap available without making it mandatory.
- Keep the LSP baseline centered on clangd and compile commands.
- Make Pages builds deterministic.

**Non-Goals:**
- Replacing existing build/test scripts.
- Adding new CI platforms or extra package managers.
- Introducing heavyweight MCP or editor dependencies.

## Decisions

### Scripts carry workflow enforcement, hooks reinforce it

The repository should expose reusable scripts first, then call them from hooks where appropriate.
That keeps the workflow available to humans and AI tools alike.

**Alternatives considered**
- Encode everything in hooks only: rejected because AI tools often need a standalone command.
- Encode everything in prose only: rejected because that is easy to ignore.

### Keep clangd as the only project-level LSP baseline

The repository already leans on clangd in VS Code settings. A root `.clangd` file makes the
baseline editor-agnostic without introducing parallel C++ language-server setups.

**Alternatives considered**
- Support multiple project-level C++ LSP stacks: rejected because it increases maintenance cost.

## Risks / Trade-offs

- **[Risk]** Strict pre-push checks can feel noisy during heavy iteration.  
  **Mitigation:** keep the hook focused on actionable repository-state checks.
- **[Risk]** Shell scripts can become Linux-centric.  
  **Mitigation:** keep the scripts simple and document them as the POSIX baseline.

## Migration Plan

1. Add reusable workflow scripts.
2. Wire the preflight script into pre-push/manual hook stages.
3. Add the root `.clangd` config and deterministic Pages workflow behavior.

Rollback is low risk: remove the scripts and hook entries if they prove too disruptive.

## Open Questions

- Should a future closeout pass fold selected checks into a dedicated CI job instead of pre-push?
