## 1. Restore the default validation gate

- [ ] 1.1 Fix the current format-check failures without widening the default gate
- [ ] 1.2 Re-establish a passing `clang-debug` unit-test baseline using the existing test scripts

## 2. Harden regression coverage

- [ ] 2.1 Keep async I/O regression tests aligned with consumed-byte position and abort semantics
- [ ] 2.2 Keep compressed stream regression tests aligned with real stream behavior for valid and
      invalid input

## 3. Verify the closeout baseline

- [ ] 3.1 Run `./scripts/lint.sh format-check`
- [ ] 3.2 Run `./scripts/test.sh clang-debug`
