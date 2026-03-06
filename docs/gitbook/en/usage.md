# Usage

## Build

```bash
conan install . --build=missing -of=build/clang-release -s build_type=Release -s compiler.cppstd=20
cmake --preset clang-release
cmake --build --preset clang-release -j$(nproc)
```

## Test

```bash
./scripts/test.sh clang-debug
```

## Common Commands

```bash
fqc compress -i input.fastq -o output.fqc
fqc decompress -i output.fqc -o restored.fastq
fqc info output.fqc
fqc verify output.fqc
```
