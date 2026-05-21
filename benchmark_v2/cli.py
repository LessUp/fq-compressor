from __future__ import annotations

import argparse
import sys
from pathlib import Path

if __package__ in {None, ""}:
    sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from benchmark_v2.manifest import load_tools, load_workloads


def main() -> None:
    parser = argparse.ArgumentParser()
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--list-workloads", action="store_true")
    group.add_argument("--list-tools", action="store_true")
    args = parser.parse_args()

    if args.list_workloads:
        for workload in load_workloads():
            print(workload.workload_id)
        return

    for tool in load_tools():
        print(tool.tool_id)


if __name__ == "__main__":
    main()
