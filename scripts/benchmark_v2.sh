#!/usr/bin/env bash

# Internal compatibility entrypoint for lower-level benchmark automation. Dataset comparisons
# documented for users go through scripts/benchmark.sh.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

exec python3 "${ROOT}/benchmark_v2/cli.py" "$@"
