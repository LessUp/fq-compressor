## Context

The repository had drifted into a state where `./scripts/lint.sh format-check` and
`./scripts/test.sh clang-debug` no longer passed. The failures were concentrated in logging macro
formatting, async reader position semantics, async writer abort expectations, and compressed-stream
regression tests.

## Goals / Non-Goals

**Goals:**
- Restore a green default validation baseline.
- Capture the baseline in a small set of deterministic checks.
- Prefer focused fixes over broad rewrites.

**Non-Goals:**
- Redesigning the I/O subsystem.
- Expanding test coverage far beyond the observed regression surface.
- Changing the public CLI or archive format.

## Decisions

### Keep the canonical developer gate small

The default gate remains `format-check` plus `clang-debug` unit tests because those checks are
already encoded in repository scripts and give fast, high-signal feedback.

**Alternatives considered**
- Add sanitizer or release builds to the default gate: rejected because they are valuable but too
  expensive for every cleanup iteration.
- Add new bespoke health scripts: rejected because the repository already has working entrypoints.

### Fix semantics where the code is wrong, relax tests where the test is wrong

The async reader position bug is an implementation bug and must be fixed in code. Several compressed
stream failures were caused by test setup and overly rigid expectations, so those are corrected in
tests instead of forcing artificial runtime behavior.

**Alternatives considered**
- Treat all failures as implementation bugs: rejected because it would encode incorrect semantics.
- Treat all failures as test bugs: rejected because the async reader position behavior was genuinely
  wrong.

## Risks / Trade-offs

- **[Risk]** The default gate can still miss slower or environment-specific regressions.  
  **Mitigation:** Keep this change narrow and let later tooling work decide where heavier checks fit.
- **[Risk]** Tightening tests around current behavior can preserve accidental behavior.  
  **Mitigation:** Only codify behavior that matches documented or user-visible semantics.

## Migration Plan

1. Fix the failing implementation and test cases.
2. Re-run `format-check` and `clang-debug` tests.
3. Use the repaired baseline as the starting point for subsequent closeout changes.

Rollback is straightforward: revert the baseline-health patch set if it introduces false confidence or
unexpected regressions.

## Open Questions

- Should future closeout work add one lightweight end-to-end roundtrip gate to the default baseline?
