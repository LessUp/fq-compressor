## 1. Consolidate the closeout track

- [ ] 1.1 Rewrite this change so it absorbs the scope of the other five closeout-oriented active
      changes
- [ ] 1.2 Preserve the absorbed scope by moving the superseded change directories into
      `openspec/changes/archive/`
- [ ] 1.3 Fix `./scripts/dev/preflight.sh` so it reports the actual active-change count from
      `openspec list --json`

## 2. Audit repository surfaces

- [ ] 2.1 Classify major repository surfaces as keep, merge, archive, remove, or rewrite
- [ ] 2.2 Publish the closeout definition of done covering validation, documentation, metadata,
      OpenSpec state, and repository hygiene

## 3. Normalize active versus archived documentation

- [ ] 3.1 Update contributor-facing docs so `openspec/specs/` is the only living requirements source
- [ ] 3.2 Mark `specs/` and archived docs as reference-only surfaces
- [ ] 3.3 Remove or archive low-value duplicated docs and checked-in generated site output

## 4. Reposition Pages and GitHub presentation

- [ ] 4.1 Rework the Pages root and navigation into a lean landing surface with EN/ZH entry paths
- [ ] 4.2 Align README, Pages, and docs ownership so they do not mirror each other
- [ ] 4.3 Update GitHub About metadata (description, homepage, topics) to match the landing story

## 5. Revalidate and finish the closeout health baseline

- [ ] 5.1 Recheck historical diagnosis and progress documents against the current codebase
- [ ] 5.2 Classify historical bug candidates as fixed, still present, stale, or unproven
- [ ] 5.3 Fix any real remaining closeout-critical defects and add focused regression coverage
- [ ] 5.4 Keep `./scripts/lint.sh format-check` and `./scripts/test.sh clang-debug` green

## 6. Simplify workflows, tooling, and AI guidance

- [ ] 6.1 Keep CI and Pages workflows aligned with existing repository scripts and deterministic
      installs
- [ ] 6.2 Unify AGENTS/CLAUDE/Copilot guidance around the same OpenSpec-first closeout workflow
- [ ] 6.3 Review project-level AI assets, LSP/editor config, and optional helpers to remove drift and
      unnecessary maintenance surface

## 7. Finalize release-readiness and handoff

- [ ] 7.1 Write the internal closeout checklist covering validation, docs, metadata, and repository
      hygiene
- [ ] 7.2 Decide which remaining items must land before archive-readiness review
- [ ] 7.3 Run `/review` on the final closeout diff set when the resulting change is risky or unusually
      broad
- [ ] 7.4 Archive this umbrella change once the repository meets the checklist
