## 1. Add scaffold and roadmap doc

- [ ] 1.1 Create `openspec/changes/introduce-closeout-roadmap/proposal.md` with why, what,
      capabilities, and impact
- [ ] 1.2 Create `openspec/changes/introduce-closeout-roadmap/design.md` with context, goals,
      decisions, risks, and migration plan
- [ ] 1.3 Create `openspec/changes/introduce-closeout-roadmap/tasks.md` with implementation
      checklist
- [ ] 1.4 Create `openspec/roadmap/closeout.md` with capability state model, phase definitions,
      and near-term change queue

## 2. Add new capability specs

- [ ] 2.1 Create `openspec/specs/docs-site.yaml` with documentation website requirements
- [ ] 2.2 Create `openspec/specs/release-engineering.yaml` with release artifact and distribution
      requirements

## 3. Add capability spec deltas

- [ ] 3.1 Add closeout-phase requirements to `openspec/specs/random-access.yaml`
- [ ] 3.2 Add closeout-phase requirements to `openspec/specs/performance.yaml`
- [ ] 3.3 Add closeout-phase requirements to `openspec/specs/compatibility.yaml`
- [ ] 3.4 Add closeout-phase requirements to `openspec/specs/build-system.yaml`

## 4. Validate and commit

- [ ] 4.1 Run `./scripts/dev/preflight.sh` to verify repository health
- [ ] 4.2 Run `openspec list --json` to confirm the change is visible
- [ ] 4.3 Run `./scripts/lint.sh format-check` to verify formatting compliance
- [ ] 4.4 Run `./scripts/test.sh clang-debug` to verify the default test gate is green
