# Conan Profiles for fq-compressor

This directory contains Conan 2.x profiles for reproducible builds.

## Available Profiles

| Profile | Compiler | Stdlib | Use Case |
|---------|----------|--------|----------|
| `default` | GCC 15 | libstdc++ | Default local development |
| `gcc` | GCC 15 | libstdc++ | Production builds |
| `clang` | Clang 22 | libc++ | Development/debugging |
| `ci-gcc` | GCC 15 | libstdc++ | CI (GitHub Actions) |
| `ci-clang` | Clang 22 | libc++ | CI (GitHub Actions) |

## Usage

### Local Development

```bash
# Using default profile (GCC)
conan install . --profile=conan/profiles/default --build=missing

# Using Clang for development
conan install . --profile=conan/profiles/clang --build=missing
```

### CI Environment

CI workflows use `scripts/install_deps.sh` which auto-detects the compiler version
and generates a temporary profile. The `ci-*` profiles serve as reference configurations.

```bash
# Via install_deps.sh (recommended for CI)
./scripts/install_deps.sh gcc-release

# Or using checked-in CI profiles directly
conan install . --profile=conan/profiles/ci-gcc --build=missing
```

## Generating Lockfiles

To generate a lockfile for reproducible builds:

```bash
# Generate lockfile
conan lock create . --profile=conan/profiles/default --lockfile-out=conan/lockfiles/conan.lock

# Install using lockfile
conan install . --lockfile=conan/lockfiles/conan.lock --build=missing
```

## Profile Customization

For local customizations, create a `~/.conan2/profiles/default` file or use
environment variables. Do not modify the checked-in profiles for local needs.
