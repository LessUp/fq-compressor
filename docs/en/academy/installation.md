# Installation

Use this page when you need the shortest path to a usable `fqc` binary.
If you want the first verified round trip after installation, continue with [Getting started](./getting-started).

## Linux release binary

Current GitHub Releases publish Linux tarballs.

```bash
wget https://github.com/LessUp/fq-compressor/releases/download/v0.2.0/fq-compressor-v0.2.0-linux-x86_64-musl.tar.gz
tar -xzf fq-compressor-v0.2.0-linux-x86_64-musl.tar.gz
sudo mv fq-compressor-v0.2.0-linux-x86_64-musl/fqc /usr/local/bin/
fqc --version
```

## Build from source

Use this path when you need the current repository state, a debug build, or local modification.

```bash
git clone https://github.com/LessUp/fq-compressor.git
cd fq-compressor
./scripts/build.sh clang-debug
build/clang-debug/src/fqc --version
```

## Toolchain requirements

- GCC 14+ or Clang 18+
- CMake 3.28+
- Conan 2.x

## Continue with

- [Getting started](./getting-started) for a smoke-scale archive round trip
- [CLI workflows](./cli-workflows) for the command surface
- [Releases](https://github.com/LessUp/fq-compressor/releases) for published artifacts
