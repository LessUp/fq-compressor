from __future__ import annotations

import gzip
from pathlib import Path

from benchmark_v2.models import WorkloadSpec


def subset_fastq(src_gz: Path, dst_fastq: Path, read_limit: int) -> Path:
    lines_needed = read_limit * 4
    with (
        gzip.open(src_gz, "rt", encoding="ascii", errors="ignore") as fin,
        dst_fastq.open("w", encoding="ascii") as fout,
    ):
        for index, line in enumerate(fin, start=1):
            if index > lines_needed:
                break
            fout.write(line)
    return dst_fastq


def prepare_workload(spec: WorkloadSpec, data_root: Path, output_dir: Path) -> list[Path]:
    output_dir.mkdir(parents=True, exist_ok=True)
    resolved = [data_root / relative for relative in spec.inputs]
    outputs = [output_dir / f"{spec.workload_id}_R1.fastq"]
    if spec.layout == "paired":
        outputs.append(output_dir / f"{spec.workload_id}_R2.fastq")
    for src, dst in zip(resolved, outputs):
        subset_fastq(src, dst, spec.read_limit)
    return outputs
