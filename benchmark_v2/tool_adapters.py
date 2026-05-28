from __future__ import annotations

import os
import shlex
import subprocess
from dataclasses import dataclass
from pathlib import Path

from benchmark_v2.models import ToolSpec

_PROJECT_ROOT = Path(__file__).resolve().parent.parent
_ARCHIVE_SUFFIXES = {
    "gzip": ".gz",
    "xz": ".xz",
    "bzip2": ".bz2",
}


@dataclass(frozen=True)
class ToolAdapter:
    tool_id: str
    supports_paired: bool
    supports_threads: bool
    archive_suffix: str

    def compress_command(self, inputs: list[Path], output: Path, threads: int) -> list[str]:
        raise NotImplementedError

    def decompress_command(
        self, archive: Path, restored_base: Path, threads: int, paired: bool
    ) -> list[str]:
        raise NotImplementedError

    def restored_outputs(self, restored_base: Path, paired: bool) -> tuple[Path, ...]:
        if not paired:
            return (restored_base,)
        return (
            restored_base.with_name(f"{restored_base.stem}_R1{restored_base.suffix}"),
            restored_base.with_name(f"{restored_base.stem}_R2{restored_base.suffix}"),
        )


@dataclass(frozen=True)
class FqcAdapter(ToolAdapter):
    binary: Path

    def __init__(self, binary: Path) -> None:
        super().__init__(
            tool_id="fqc", supports_paired=True, supports_threads=True, archive_suffix=".fqc"
        )
        object.__setattr__(self, "binary", binary)

    def compress_command(self, inputs: list[Path], output: Path, threads: int) -> list[str]:
        command = [
            str(self.binary),
            "-t",
            str(threads),
            "-q",
            "compress",
            "-i",
            str(inputs[0]),
            "-o",
            str(output),
        ]
        if len(inputs) == 2:
            command.extend(["-2", str(inputs[1]), "--paired"])
        return command

    def decompress_command(
        self, archive: Path, restored_base: Path, threads: int, paired: bool
    ) -> list[str]:
        command = [
            str(self.binary),
            "-t",
            str(threads),
            "-q",
            "decompress",
            "-i",
            str(archive),
            "-o",
            str(restored_base),
        ]
        if paired:
            command.extend(["--split-pe", "--original-order"])
        else:
            command.append("--original-order")
        return command


@dataclass(frozen=True)
class TemplateToolAdapter(ToolAdapter):
    compress_template: str
    decompress_template: str

    def __init__(self, spec: ToolSpec) -> None:
        super().__init__(
            tool_id=spec.tool_id,
            supports_paired=spec.supports_paired,
            supports_threads=False,
            archive_suffix=_ARCHIVE_SUFFIXES.get(spec.tool_id, f".{spec.tool_id}"),
        )
        object.__setattr__(self, "compress_template", spec.compress_template)
        object.__setattr__(self, "decompress_template", spec.decompress_template)

    def compress_command(self, inputs: list[Path], output: Path, threads: int) -> list[str]:
        del threads
        if len(inputs) != 1:
            raise ValueError(f"tool '{self.tool_id}' supports only single-file inputs")
        return _render_shell_command(self.compress_template, input_path=inputs[0], output_path=output)

    def decompress_command(
        self, archive: Path, restored_base: Path, threads: int, paired: bool
    ) -> list[str]:
        del threads
        if paired:
            raise ValueError(f"tool '{self.tool_id}' does not support paired workloads")
        return _render_shell_command(
            self.decompress_template, input_path=archive, output_path=restored_base
        )


def _render_shell_command(template: str, *, input_path: Path, output_path: Path) -> list[str]:
    command = template.format(
        input=shlex.quote(str(input_path)),
        output=shlex.quote(str(output_path)),
    )
    return ["bash", "-c", command]


def _shared_project_root() -> Path | None:
    try:
        result = subprocess.run(
            ["git", "-C", str(_PROJECT_ROOT), "rev-parse", "--git-common-dir"],
            capture_output=True,
            text=True,
            timeout=5,
            check=False,
        )
    except Exception:
        return None
    if result.returncode != 0:
        return None
    common_dir = Path(result.stdout.strip()).resolve()
    if common_dir.name != ".git":
        return None
    root = common_dir.parent
    if root == _PROJECT_ROOT:
        return None
    return root


def _resolve_fqc_binary() -> Path:
    explicit = os.environ.get("FQC_BENCHMARK_BINARY")
    if explicit:
        candidate = Path(explicit).expanduser()
        if candidate.exists() and os.access(candidate, os.X_OK):
            return candidate.resolve()
        raise ValueError(f"FQC_BENCHMARK_BINARY is not executable: {candidate}")

    roots = [_PROJECT_ROOT]
    shared_root = _shared_project_root()
    if shared_root is not None:
        roots.append(shared_root)

    candidates: list[Path] = []
    for root in roots:
        candidates.extend(
            (
                root / "build/clang-release/src/fqc",
                root / "build/clang-debug/src/fqc",
            )
        )
    existing = [candidate for candidate in candidates if candidate.exists()]
    if existing:
        return max(existing, key=lambda candidate: candidate.stat().st_mtime)
    raise ValueError("fqc binary not found under build/clang-release/src/fqc or build/clang-debug/src/fqc")


def create_adapter(spec: ToolSpec) -> ToolAdapter:
    if spec.tool_id == "fqc":
        return FqcAdapter(_resolve_fqc_binary())
    return TemplateToolAdapter(spec)
