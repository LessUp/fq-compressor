#!/usr/bin/env python3
"""
Compiler Benchmark Framework
=============================
Compare fq-compressor performance between GCC and Clang builds.

Usage:
    python compiler_benchmark.py --input data.fastq.gz \
        --gcc-binary build/gcc-release/src/fqc \
        --clang-binary build/clang-release/src/fqc \
        --report reports/compiler_comparison.md
"""

import argparse
import json
import os
import shutil
import subprocess
import tempfile
import time
from dataclasses import dataclass, asdict
from datetime import datetime
from pathlib import Path
from typing import Optional


@dataclass
class CompilerBenchmarkResult:
    """Single benchmark result for a compiler."""
    compiler: str
    operation: str
    input_size: int
    output_size: int
    elapsed_seconds: float
    threads: int
    success: bool
    error: Optional[str] = None
    peak_memory_mb: float = 0.0

    @property
    def ratio(self) -> float:
        if self.input_size == 0:
            return 0.0
        return self.output_size / self.input_size

    @property
    def speed_mb_s(self) -> float:
        if self.elapsed_seconds == 0:
            return 0.0
        return (self.input_size / 1024 / 1024) / self.elapsed_seconds


class CompilerBenchmark:
    """Run benchmarks comparing GCC and Clang builds."""

    def __init__(self, gcc_binary: str, clang_binary: str):
        self.gcc_binary = gcc_binary
        self.clang_binary = clang_binary
        self.results: list[CompilerBenchmarkResult] = []

    def _get_file_size(self, path: str) -> int:
        try:
            return os.path.getsize(path)
        except OSError:
            return 0

    def _run_timed_command(self, cmd: str, timeout: int = 3600) -> tuple[float, bool, str, float]:
        """Run command and return (elapsed, success, error, peak_memory_mb)."""
        start = time.perf_counter()
        peak_memory = 0.0
        try:
            # Use /usr/bin/time to measure memory if available
            time_cmd = f"/usr/bin/time -v {cmd} 2>&1"
            result = subprocess.run(
                time_cmd, shell=True, capture_output=True, text=True, timeout=timeout
            )
            elapsed = time.perf_counter() - start

            # Extract peak memory from time output
            for line in result.stderr.split('\n'):
                if 'Maximum resident set size' in line:
                    try:
                        peak_memory = float(line.split(':')[1].strip()) / 1024  # KB to MB
                    except (ValueError, IndexError):
                        pass

            if result.returncode != 0:
                return elapsed, False, result.stderr[:200], peak_memory
            return elapsed, True, "", peak_memory
        except subprocess.TimeoutExpired:
            return timeout, False, "Timeout", peak_memory
        except Exception as e:
            return 0, False, str(e), peak_memory

    def _decompress_if_needed(self, input_file: str, work_dir: Path) -> str:
        """Decompress .gz file if needed."""
        if input_file.endswith('.gz'):
            decompressed = work_dir / Path(input_file).stem
            if not decompressed.exists():
                print(f"  Decompressing {input_file}...")
                cmd = f"zcat {input_file} > {decompressed}"
                result = subprocess.run(cmd, shell=True, capture_output=True)
                if result.returncode != 0:
                    raise RuntimeError(f"Failed to decompress: {result.stderr}")
            return str(decompressed)
        return input_file

    def benchmark_binary(
        self,
        compiler: str,
        binary: str,
        input_file: str,
        threads: int = 4,
        work_dir: Path = None
    ) -> list[CompilerBenchmarkResult]:
        """Benchmark a single binary."""
        results = []

        if work_dir is None:
            work_dir = Path(tempfile.mkdtemp())
            cleanup = True
        else:
            cleanup = False

        try:
            # Decompress if needed
            actual_input = self._decompress_if_needed(input_file, work_dir)
            input_size = self._get_file_size(actual_input)

            basename = Path(actual_input).stem
            compressed = work_dir / f"{basename}_{compiler}.fqc"
            decompressed = work_dir / f"{basename}_{compiler}_dec.fastq"

            # Compression benchmark
            compress_cmd = f"{binary} compress -i {actual_input} -o {compressed} -t {threads} -q"
            elapsed, success, error, memory = self._run_timed_command(compress_cmd)

            compressed_size = self._get_file_size(str(compressed)) if success else 0

            results.append(CompilerBenchmarkResult(
                compiler=compiler,
                operation="compress",
                input_size=input_size,
                output_size=compressed_size,
                elapsed_seconds=elapsed,
                threads=threads,
                success=success,
                error=error if not success else None,
                peak_memory_mb=memory
            ))

            # Decompression benchmark
            if success and compressed.exists():
                decompress_cmd = f"{binary} decompress -i {compressed} -o {decompressed} -t {threads} -q"
                elapsed, success, error, memory = self._run_timed_command(decompress_cmd)

                decompressed_size = self._get_file_size(str(decompressed)) if success else 0

                results.append(CompilerBenchmarkResult(
                    compiler=compiler,
                    operation="decompress",
                    input_size=compressed_size,
                    output_size=decompressed_size,
                    elapsed_seconds=elapsed,
                    threads=threads,
                    success=success,
                    error=error if not success else None,
                    peak_memory_mb=memory
                ))

        finally:
            if cleanup:
                shutil.rmtree(work_dir, ignore_errors=True)

        return results

    def run_comparison(
        self,
        input_file: str,
        threads_list: list[int] = None,
        runs: int = 3,
        verbose: bool = True
    ) -> dict:
        """Run full comparison between GCC and Clang."""
        if threads_list is None:
            threads_list = [1, 4, 8]

        input_size = self._get_file_size(input_file)

        if verbose:
            print(f"\n{'='*60}")
            print("Compiler Performance Comparison: GCC vs Clang")
            print(f"{'='*60}")
            print(f"Input: {input_file}")
            print(f"Size: {input_size / 1024 / 1024:.2f} MB (compressed)")
            print(f"Threads: {threads_list}")
            print(f"Runs: {runs}")
            print(f"{'='*60}\n")

        # Create shared work directory
        work_dir = Path(tempfile.mkdtemp())

        try:
            compilers = [
                ("GCC", self.gcc_binary),
                ("Clang", self.clang_binary)
            ]

            for compiler_name, binary in compilers:
                if not Path(binary).exists():
                    if verbose:
                        print(f"[SKIP] {compiler_name}: Binary not found at {binary}")
                    continue

                if verbose:
                    print(f"\n[{compiler_name}] {binary}")

                for threads in threads_list:
                    best_compress = None
                    best_decompress = None

                    for run in range(runs):
                        if verbose:
                            print(f"  Run {run+1}/{runs} (threads={threads})...", end=" ", flush=True)

                        results = self.benchmark_binary(
                            compiler_name, binary, input_file, threads, work_dir
                        )

                        for r in results:
                            if r.operation == "compress":
                                if best_compress is None or r.elapsed_seconds < best_compress.elapsed_seconds:
                                    best_compress = r
                            else:
                                if best_decompress is None or r.elapsed_seconds < best_decompress.elapsed_seconds:
                                    best_decompress = r

                        if verbose and results:
                            cr = next((r for r in results if r.operation == "compress"), None)
                            if cr and cr.success:
                                print(f"ratio={cr.ratio:.4f}, speed={cr.speed_mb_s:.1f} MB/s")
                            elif cr:
                                print(f"FAILED: {cr.error}")

                    if best_compress:
                        self.results.append(best_compress)
                    if best_decompress:
                        self.results.append(best_decompress)

        finally:
            shutil.rmtree(work_dir, ignore_errors=True)

        return {
            "timestamp": datetime.now().isoformat(),
            "input_file": input_file,
            "input_size": input_size,
            "threads_tested": threads_list,
            "results": [asdict(r) for r in self.results]
        }

    def generate_report(self, output_path: str = None) -> str:
        """Generate markdown comparison report."""
        lines = [
            "# GCC vs Clang 编译器性能对比报告",
            "",
            f"**生成时间:** {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}",
            "",
            "## 测试环境",
            "",
        ]

        # Get compiler versions
        for name, binary in [("GCC", self.gcc_binary), ("Clang", self.clang_binary)]:
            if Path(binary).exists():
                lines.append(f"- **{name} Binary:** `{binary}`")

        lines.extend([
            "",
            "## 压缩性能对比",
            "",
            "| 编译器 | 线程数 | 压缩率 | 速度 (MB/s) | 内存 (MB) |",
            "|--------|--------|--------|-------------|-----------|",
        ])

        compress_results = [r for r in self.results if r.operation == "compress" and r.success]
        for r in sorted(compress_results, key=lambda x: (x.compiler, x.threads)):
            lines.append(
                f"| {r.compiler} | {r.threads} | {r.ratio:.4f} | {r.speed_mb_s:.1f} | {r.peak_memory_mb:.0f} |"
            )

        lines.extend([
            "",
            "## 解压性能对比",
            "",
            "| 编译器 | 线程数 | 速度 (MB/s) | 内存 (MB) |",
            "|--------|--------|-------------|-----------|",
        ])

        decompress_results = [r for r in self.results if r.operation == "decompress" and r.success]
        for r in sorted(decompress_results, key=lambda x: (x.compiler, x.threads)):
            # For decompress, speed is based on compressed size input
            speed = (r.output_size / 1024 / 1024) / r.elapsed_seconds if r.elapsed_seconds > 0 else 0
            lines.append(
                f"| {r.compiler} | {r.threads} | {speed:.1f} | {r.peak_memory_mb:.0f} |"
            )

        # Summary
        lines.extend([
            "",
            "## 总结",
            "",
        ])

        gcc_compress = [r for r in compress_results if r.compiler == "GCC"]
        clang_compress = [r for r in compress_results if r.compiler == "Clang"]

        if gcc_compress and clang_compress:
            gcc_best = max(gcc_compress, key=lambda x: x.speed_mb_s)
            clang_best = max(clang_compress, key=lambda x: x.speed_mb_s)

            if gcc_best.speed_mb_s > clang_best.speed_mb_s:
                diff = (gcc_best.speed_mb_s - clang_best.speed_mb_s) / clang_best.speed_mb_s * 100
                lines.append(f"- **GCC** 在压缩速度上快 **{diff:.1f}%**")
            else:
                diff = (clang_best.speed_mb_s - gcc_best.speed_mb_s) / gcc_best.speed_mb_s * 100
                lines.append(f"- **Clang** 在压缩速度上快 **{diff:.1f}%**")

        lines.extend([
            "",
            "---",
            "*由 fq-compressor 编译器对比框架生成*"
        ])

        report = "\n".join(lines)

        if output_path:
            Path(output_path).parent.mkdir(parents=True, exist_ok=True)
            with open(output_path, 'w') as f:
                f.write(report)

        return report

    def export_json(self, data: dict, output_path: str):
        """Export results to JSON."""
        Path(output_path).parent.mkdir(parents=True, exist_ok=True)
        with open(output_path, 'w') as f:
            json.dump(data, f, indent=2)


def main():
    parser = argparse.ArgumentParser(
        description="Compare fq-compressor performance between GCC and Clang builds"
    )
    parser.add_argument("-i", "--input", required=True, help="Input FASTQ file")
    parser.add_argument("--gcc-binary", default="build/gcc-release/src/fqc",
                        help="Path to GCC-built binary")
    parser.add_argument("--clang-binary", default="build/clang-release/src/fqc",
                        help="Path to Clang-built binary")
    parser.add_argument("-t", "--threads", type=int, nargs="+", default=[1, 4, 8],
                        help="Thread counts to test")
    parser.add_argument("-r", "--runs", type=int, default=3, help="Runs per config")
    parser.add_argument("--report", help="Output markdown report path")
    parser.add_argument("--json", help="Output JSON results path")
    parser.add_argument("--output-dir", help="Output directory for reports and visualizations")
    parser.add_argument("--visualize", action="store_true",
                        help="Generate visualization charts and reports")
    parser.add_argument("-q", "--quiet", action="store_true", help="Quiet mode")

    args = parser.parse_args()

    benchmark = CompilerBenchmark(args.gcc_binary, args.clang_binary)

    data = benchmark.run_comparison(
        args.input,
        threads_list=args.threads,
        runs=args.runs,
        verbose=not args.quiet
    )

    # 确定输出目录
    output_dir = args.output_dir if args.output_dir else "docs/benchmark"

    # 保存 JSON（如果启用可视化，必须保存）
    json_path = args.json
    if args.visualize and not json_path:
        json_path = str(Path(output_dir) / "benchmark_results.json")

    if json_path:
        benchmark.export_json(data, json_path)
        if not args.quiet:
            print(f"\nJSON saved: {json_path}")

    # 生成文本报告
    if args.report:
        benchmark.generate_report(args.report)
        if not args.quiet:
            print(f"Report saved: {args.report}")

    # 生成可视化
    if args.visualize:
        try:
            # 动态导入可视化模块
            from visualize_benchmark import BenchmarkVisualizer

            if not args.quiet:
                print("\n" + "="*60)
                print("生成可视化报告...")
                print("="*60)

            visualizer = BenchmarkVisualizer(data)
            visualizer.generate_all_charts()

            output_path = Path(output_dir)
            visualizer.save_figures(output_path)
            visualizer.generate_markdown_report(output_path)
            visualizer.generate_html_report(output_path)

            if not args.quiet:
                print(f"\n✓ 可视化报告已保存到: {output_dir}")

        except ImportError as e:
            print(f"\n警告: 无法导入可视化模块: {e}")
            print("请确保 visualize_benchmark.py 在 Python 路径中")
        except Exception as e:
            print(f"\n错误: 可视化生成失败: {e}")

    if not args.quiet and not args.report and not args.visualize:
        print("\n" + benchmark.generate_report())


if __name__ == "__main__":
    main()
