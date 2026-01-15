# Conan Profiles for fq-compressor

This directory contains Conan 2.x profiles for reproducible builds.

## Available Profiles

| Profile | Compiler | Stdlib | Use Case |
|---------|----------|--------|----------|
| `default` | GCC 15 | libstdc++ | Default local development |
| `gcc` | GCC 15 | libstdc++ | Production builds |
| `clang` | Clang 21 | libc++ | Development/debugging |
| `ci-gcc` | GCC 13 | libstdc++ | CI (GitHub Actions) |
| `ci-clang` | Clang 21 | libc++ | CI (GitHub Actions) |

## Usage

### Local Development

```bash
# Using default profile (GCC)
conan install . --profile=conan/profiles/default --build=missing

# Using Clang for development
conan install . --profile=conan/profiles/clang --build=missing
```

### CI Environment

```bash
# CI uses auto-detected profile or specific CI profiles
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
