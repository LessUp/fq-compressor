## 1. Add scaffold and roadmap doc

- [x] 1.1 Create `openspec/changes/introduce-closeout-roadmap/proposal.md` with why, what,
      capabilities, and impact
- [x] 1.2 Create `openspec/changes/introduce-closeout-roadmap/design.md` with context, goals,
      decisions, risks, and migration plan
- [x] 1.3 Create `openspec/changes/introduce-closeout-roadmap/tasks.md` with implementation
      checklist
- [x] 1.4 Create `openspec/roadmap/closeout.md` with capability state model, phase definitions,
      and near-term change queue

## 2. Add new capability specs

- [x] 2.1 Create `openspec/changes/introduce-closeout-roadmap/specs/docs-site/spec.md` with
      documentation website requirements
- [x] 2.2 Create `openspec/changes/introduce-closeout-roadmap/specs/release-engineering/spec.md`
      with release artifact and distribution requirements

## 3. Add capability spec deltas

- [x] 3.1 Add closeout-phase requirements to
      `openspec/changes/introduce-closeout-roadmap/specs/random-access/spec.md`
- [x] 3.2 Add closeout-phase requirements to
      `openspec/changes/introduce-closeout-roadmap/specs/performance/spec.md`
- [x] 3.3 Add closeout-phase requirements to
      `openspec/changes/introduce-closeout-roadmap/specs/compatibility/spec.md`
- [x] 3.4 Add closeout-phase requirements to
      `openspec/changes/introduce-closeout-roadmap/specs/build-system/spec.md`

## 4. Validate and commit

- [x] 4.1 Run `./scripts/dev/preflight.sh` to verify repository health
- [x] 4.2 Run `openspec list --json` to confirm the change is visible
- [x] 4.3 Run `./scripts/lint.sh format-check` to verify formatting compliance
- [x] 4.4 Run `./scripts/test.sh clang-debug` to verify the default test gate is green
