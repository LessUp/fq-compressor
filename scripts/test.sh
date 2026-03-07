#!/usr/bin/env bash
# scripts/test.sh - 测试运行脚本（简化入口，委托给 run_tests.sh）
#
# Usage:
#   ./scripts/test.sh [preset] [filter]
#
# For advanced options (--label, --verbose, --build-only), use run_tests.sh directly.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

PRESET="${1:-clang-debug}"
FILTER="${2:-}"

ARGS=(-p "$PRESET")
if [ -n "$FILTER" ]; then
    ARGS+=(-f "$FILTER")
fi

exec "$SCRIPT_DIR/run_tests.sh" "${ARGS[@]}"
