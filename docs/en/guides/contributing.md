# Contributing

fq-compressor is in **closeout mode**. Prefer focused fixes, documentation tightening, evidence-backed bug work, and simplification over broad feature expansion.

## Start from the current branch tip

```bash
git clone https://github.com/LessUp/fq-compressor.git
cd fq-compressor
git checkout master
```

If you are contributing through a fork or pull request flow, branch from `master`.

## Recommended local workflow

```bash
./scripts/build.sh clang-debug
./scripts/test.sh clang-debug
./scripts/lint.sh format-check
```

Use `./scripts/lint.sh lint clang-debug` when your change touches C++ code paths that need static-analysis confirmation.

## Keep changes small and reviewable

Prioritize work that fits the current project posture:

- tighten command behavior or diagnostics
- improve benchmark evidence or reproducibility
- simplify tooling and documentation
- fix correctness issues with regression coverage

Avoid changing public claims unless the repository evidence also changes.

## Documentation expectations

Update the surface you changed:

- GitHub Pages content under `docs/en/` or `docs/zh/`
- repository-facing references such as `README.md`, `AGENTS.md`, or `docs/benchmark/README.md`
- links to repository files should target the `master` branch

## Before you hand off a change

```bash
git --no-pager diff --stat
./scripts/test.sh clang-debug
./scripts/lint.sh format-check
```

For documentation-only work, a successful `cd docs && npm run build` is the relevant public-site check.

## Continue with

- [Getting started](./getting-started) for a fresh local setup
- [CLI usage](./cli-usage) for command examples used in bug reports and reviews
- [Resources](/en/resources/) for specs, benchmark artifacts, and reference material
