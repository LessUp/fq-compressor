#!/usr/bin/env python3
"""
fq-compressor Benchmark Framework
==================================
Comprehensive benchmarking tool for comparing FASTQ compression tools.

Usage:
    python benchmark.py --input data.fastq --tools fqc,spring,gzip --threads 4
    python benchmark.py --input data.fastq --all --report reports/benchmark.md
"""

import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time
from dataclasses import dataclass, field, asdict
from datetime import datetime
from pathlib import Path
from typing import Optional

import yaml


@dataclass
class BenchmarkResult:
    """Result of a single benchmark run."""
    tool: str
    operation: str  # compress or decompress
    input_size: int
    output_size: int
    elapsed_seconds: float
    threads: int
    success: bool
    error: Optional[str] = None
    
    @property
    def ratio(self) -> float:
        """Compression ratio (compressed/original)."""
        if self.input_size == 0:
            return 0.0
        return self.output_size / self.input_size
    
    @property
    def speed_mb_s(self) -> float:
        """Processing speed in MB/s."""
        if self.elapsed_seconds == 0:
            return 0.0
        return (self.input_size / 1024 / 1024) / self.elapsed_seconds
    
    @property
    def bits_per_base(self) -> float:
        """Bits per base (assuming ~0.5 bytes per base in FASTQ)."""
        if self.input_size == 0:
            return 0.0
        # FASTQ: ~4 lines per read, base takes ~0.5 of file size
        estimated_bases = self.input_size * 0.5
        return (self.output_size * 8) / estimated_bases


@dataclass
class ToolConfig:
    """Configuration for a compression tool."""
    name: str
    compress: str
    decompress: str
    extension: str
    category: str
    description: str
    version_cmd: str = ""
    version: str = "unknown"


@dataclass
class BenchmarkSuite:
    """Complete benchmark suite results."""
    timestamp: str
    input_file: str
    input_size: int
    tools_tested: list
    results: list = field(default_factory=list)
    system_info: dict = field(default_factory=dict)


class BenchmarkRunner:
    """Main benchmark runner class."""
    
    def __init__(self, config_path: str = None):
        self.config_path = config_path or Path(__file__).parent / "tools.yaml"
        self.tools: dict[str, ToolConfig] = {}
        self.settings = {}
        self.fqc_binary = self._find_fqc_binary()
        self._load_config()
    
    def _find_fqc_binary(self) -> str:
        """Find fqc binary path."""
        # Check common locations
        candidates = [
            Path(__file__).parent.parent / "build/build/Release/src/fqc",
            Path(__file__).parent.parent / "build/Release/src/fqc",
            Path(__file__).parent.parent / "build/src/fqc",
            shutil.which("fqc"),
        ]
        for candidate in candidates:
            if candidate and Path(candidate).exists():
                return str(candidate)
        return "fqc"  # Fall back to PATH
    
    def _load_config(self):
        """Load tool configurations from YAML."""
        with open(self.config_path, 'r') as f:
            config = yaml.safe_load(f)
        
        self.settings = config.get('settings', {})
        
        for tool_id, tool_cfg in config.get('tools', {}).items():
            self.tools[tool_id] = ToolConfig(
                name=tool_cfg.get('name', tool_id),
                compress=tool_cfg.get('compress', ''),
                decompress=tool_cfg.get('decompress', ''),
                extension=tool_cfg.get('extension', ''),
                category=tool_cfg.get('category', 'unknown'),
                description=tool_cfg.get('description', ''),
                version_cmd=tool_cfg.get('version_cmd', ''),
            )
    
    def _get_tool_version(self, tool: ToolConfig) -> str:
        """Get tool version string."""
        if not tool.version_cmd:
            return "unknown"
        try:
            cmd = tool.version_cmd.replace("fqc", self.fqc_binary)
            result = subprocess.run(
                cmd, shell=True, capture_output=True, text=True, timeout=5
            )
            version = result.stdout.strip() or result.stderr.strip()
            return version.split('\n')[0][:50]  # First line, max 50 chars
        except Exception:
            return "unknown"
    
    def _check_tool_available(self, tool_id: str) -> bool:
        """Check if a tool is available on the system."""
        tool = self.tools.get(tool_id)
        if not tool:
            return False
        
        # Extract the command name
        cmd_parts = tool.compress.split()[0]
        if cmd_parts == "fqc":
            return Path(self.fqc_binary).exists() or shutil.which("fqc") is not None
        return shutil.which(cmd_parts) is not None
    
    def _run_command(self, cmd: str, timeout: int = 3600) -> tuple[float, bool, str]:
        """Run a command and return (elapsed_time, success, error)."""
        start = time.perf_counter()
        try:
            result = subprocess.run(
                cmd, shell=True, capture_output=True, text=True, timeout=timeout
            )
            elapsed = time.perf_counter() - start
            if result.returncode != 0:
                return elapsed, False, result.stderr[:200]
            return elapsed, True, ""
        except subprocess.TimeoutExpired:
            return timeout, False, "Timeout"
        except Exception as e:
            return 0, False, str(e)
    
    def _get_file_size(self, path: str) -> int:
        """Get file size in bytes."""
        try:
            return os.path.getsize(path)
        except OSError:
            return 0
    
    def _get_system_info(self) -> dict:
        """Collect system information."""
        info = {}
        try:
            result = subprocess.run(
                "nproc", shell=True, capture_output=True, text=True
            )
            info['cpu_cores'] = int(result.stdout.strip())
        except Exception:
            info['cpu_cores'] = os.cpu_count() or 1
        
        try:
            result = subprocess.run(
                "cat /proc/meminfo | grep MemTotal", 
                shell=True, capture_output=True, text=True
            )
            mem_kb = int(result.stdout.split()[1])
            info['memory_gb'] = round(mem_kb / 1024 / 1024, 1)
        except Exception:
            info['memory_gb'] = 0
        
        try:
            result = subprocess.run(
                "uname -r", shell=True, capture_output=True, text=True
            )
            info['kernel'] = result.stdout.strip()
        except Exception:
            pass
        
        return info
    
    def benchmark_tool(
        self, 
        tool_id: str, 
        input_file: str, 
        threads: int = 1,
        work_dir: str = None,
        verify: bool = True
    ) -> list[BenchmarkResult]:
        """Run benchmark for a single tool."""
        tool = self.tools.get(tool_id)
        if not tool:
            return []
        
        results = []
        input_size = self._get_file_size(input_file)
        
        # Create temp directory for outputs
        if work_dir:
            temp_dir = Path(work_dir)
            temp_dir.mkdir(parents=True, exist_ok=True)
            cleanup = False
        else:
            temp_dir = Path(tempfile.mkdtemp())
            cleanup = True
        
        try:
            # Output file paths
            basename = Path(input_file).stem
            compressed = temp_dir / f"{basename}{tool.extension}"
            decompressed = temp_dir / f"{basename}_decompressed.fastq"
            
            # Build compress command
            compress_cmd = tool.compress.format(
                input=input_file,
                output=str(compressed),
                threads=threads
            ).replace("fqc", self.fqc_binary)
            
            # Run compression
            elapsed, success, error = self._run_command(compress_cmd)
            
            compressed_size = self._get_file_size(str(compressed)) if success else 0
            
            results.append(BenchmarkResult(
                tool=tool_id,
                operation="compress",
                input_size=input_size,
                output_size=compressed_size,
                elapsed_seconds=elapsed,
                threads=threads,
                success=success,
                error=error if not success else None
            ))
            
            # Run decompression if compression succeeded
            if success and compressed.exists():
                decompress_cmd = tool.decompress.format(
                    input=input_file,
                    output=str(compressed),
                    decompressed=str(decompressed),
                    threads=threads
                ).replace("fqc", self.fqc_binary)
                
                elapsed, success, error = self._run_command(decompress_cmd)
                
                decompressed_size = self._get_file_size(str(decompressed)) if success else 0
                
                results.append(BenchmarkResult(
                    tool=tool_id,
                    operation="decompress",
                    input_size=compressed_size,
                    output_size=decompressed_size,
                    elapsed_seconds=elapsed,
                    threads=threads,
                    success=success,
                    error=error if not success else None
                ))
                
                # Verify decompression if requested
                if verify and success and decompressed.exists():
                    # Simple size check (exact match for lossless)
                    if decompressed_size != input_size:
                        results[-1].error = f"Size mismatch: {decompressed_size} vs {input_size}"
        
        finally:
            if cleanup:
                shutil.rmtree(temp_dir, ignore_errors=True)
        
        return results
    
    def run_benchmark(
        self,
        input_file: str,
        tool_ids: list[str] = None,
        threads_list: list[int] = None,
        runs: int = 3,
        verbose: bool = True
    ) -> BenchmarkSuite:
        """Run complete benchmark suite."""
        
        if tool_ids is None:
            tool_ids = list(self.tools.keys())
        
        if threads_list is None:
            threads_list = self.settings.get('default_threads', [1, 4])
        
        # Filter to available tools
        available_tools = [t for t in tool_ids if self._check_tool_available(t)]
        unavailable = set(tool_ids) - set(available_tools)
        
        if verbose and unavailable:
            print(f"Skipping unavailable tools: {', '.join(unavailable)}")
        
        # Get tool versions
        for tool_id in available_tools:
            self.tools[tool_id].version = self._get_tool_version(self.tools[tool_id])
        
        suite = BenchmarkSuite(
            timestamp=datetime.now().isoformat(),
            input_file=input_file,
            input_size=self._get_file_size(input_file),
            tools_tested=available_tools,
            system_info=self._get_system_info()
        )
        
        if verbose:
            print(f"\n{'='*60}")
            print(f"fq-compressor Benchmark Suite")
            print(f"{'='*60}")
            print(f"Input: {input_file}")
            print(f"Size: {suite.input_size / 1024 / 1024:.2f} MB")
            print(f"Tools: {', '.join(available_tools)}")
            print(f"Threads: {threads_list}")
            print(f"Runs per config: {runs}")
            print(f"{'='*60}\n")
        
        total_configs = len(available_tools) * len(threads_list) * runs
        current = 0
        
        for tool_id in available_tools:
            tool = self.tools[tool_id]
            if verbose:
                print(f"\n[{tool.name}] v{tool.version}")
            
            for threads in threads_list:
                best_compress = None
                best_decompress = None
                
                for run in range(runs):
                    current += 1
                    if verbose:
                        print(f"  Run {run+1}/{runs} (threads={threads})...", end=" ", flush=True)
                    
                    results = self.benchmark_tool(tool_id, input_file, threads)
                    
                    for r in results:
                        if r.operation == "compress":
                            if best_compress is None or r.elapsed_seconds < best_compress.elapsed_seconds:
                                best_compress = r
                        else:
                            if best_decompress is None or r.elapsed_seconds < best_decompress.elapsed_seconds:
                                best_decompress = r
                    
                    if verbose:
                        compress_r = next((r for r in results if r.operation == "compress"), None)
                        if compress_r and compress_r.success:
                            print(f"ratio={compress_r.ratio:.4f}, speed={compress_r.speed_mb_s:.1f} MB/s")
                        elif compress_r:
                            print(f"FAILED: {compress_r.error}")
                        else:
                            print("no result")
                
                # Store best results
                if best_compress:
                    suite.results.append(best_compress)
                if best_decompress:
                    suite.results.append(best_decompress)
        
        return suite
    
    def generate_report(self, suite: BenchmarkSuite, output_path: str = None) -> str:
        """Generate markdown benchmark report."""
        
        lines = [
            "# fq-compressor Benchmark Report",
            "",
            f"**Generated:** {suite.timestamp}",
            "",
            "## Test Environment",
            "",
            f"- **Input File:** `{suite.input_file}`",
            f"- **Input Size:** {suite.input_size / 1024 / 1024:.2f} MB",
            f"- **CPU Cores:** {suite.system_info.get('cpu_cores', 'N/A')}",
            f"- **Memory:** {suite.system_info.get('memory_gb', 'N/A')} GB",
            f"- **Kernel:** {suite.system_info.get('kernel', 'N/A')}",
            "",
            "## Tools Tested",
            "",
        ]
        
        for tool_id in suite.tools_tested:
            tool = self.tools.get(tool_id)
            if tool:
                lines.append(f"- **{tool.name}** ({tool.category}): {tool.description}")
        
        lines.extend([
            "",
            "## Compression Results",
            "",
            "| Tool | Threads | Ratio | Bits/Base | Compress (MB/s) | Decompress (MB/s) |",
            "|------|---------|-------|-----------|-----------------|-------------------|",
        ])
        
        # Group results by tool and threads
        compress_results = {}
        decompress_results = {}
        
        for r in suite.results:
            key = (r.tool, r.threads)
            if r.operation == "compress" and r.success:
                compress_results[key] = r
            elif r.operation == "decompress" and r.success:
                decompress_results[key] = r
        
        for key in sorted(compress_results.keys(), key=lambda x: (x[0], x[1])):
            tool_id, threads = key
            tool = self.tools.get(tool_id)
            c = compress_results.get(key)
            d = decompress_results.get(key)
            
            if c:
                d_speed = f"{d.speed_mb_s:.1f}" if d else "N/A"
                lines.append(
                    f"| {tool.name} | {threads} | {c.ratio:.4f} | {c.bits_per_base:.2f} | "
                    f"{c.speed_mb_s:.1f} | {d_speed} |"
                )
        
        # Add summary
        lines.extend([
            "",
            "## Summary",
            "",
        ])
        
        # Find best compression ratio and speed
        best_ratio = min((r for r in suite.results if r.operation == "compress" and r.success), 
                         key=lambda x: x.ratio, default=None)
        best_speed = max((r for r in suite.results if r.operation == "compress" and r.success),
                         key=lambda x: x.speed_mb_s, default=None)
        
        if best_ratio:
            tool = self.tools.get(best_ratio.tool)
            lines.append(f"- **Best Compression Ratio:** {tool.name} ({best_ratio.ratio:.4f}, {best_ratio.bits_per_base:.2f} bits/base)")
        
        if best_speed:
            tool = self.tools.get(best_speed.tool)
            lines.append(f"- **Fastest Compression:** {tool.name} ({best_speed.speed_mb_s:.1f} MB/s)")
        
        lines.extend([
            "",
            "---",
            f"*Report generated by fq-compressor benchmark framework*",
        ])
        
        report = "\n".join(lines)
        
        if output_path:
            Path(output_path).parent.mkdir(parents=True, exist_ok=True)
            with open(output_path, 'w') as f:
                f.write(report)
        
        return report
    
    def export_json(self, suite: BenchmarkSuite, output_path: str):
        """Export results to JSON."""
        data = {
            "timestamp": suite.timestamp,
            "input_file": suite.input_file,
            "input_size": suite.input_size,
            "system_info": suite.system_info,
            "tools": {
                tid: {
                    "name": t.name,
                    "version": t.version,
                    "category": t.category,
                    "description": t.description
                }
                for tid, t in self.tools.items() if tid in suite.tools_tested
            },
            "results": [asdict(r) for r in suite.results]
        }
        
        Path(output_path).parent.mkdir(parents=True, exist_ok=True)
        with open(output_path, 'w') as f:
            json.dump(data, f, indent=2)


def main():
    parser = argparse.ArgumentParser(
        description="fq-compressor Benchmark Framework",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Benchmark fqc and gzip with 4 threads
  python benchmark.py --input data.fastq --tools fqc,gzip -t 4

  # Benchmark all available tools
  python benchmark.py --input data.fastq --all

  # Generate report
  python benchmark.py --input data.fastq --all --report reports/benchmark.md
        """
    )
    
    parser.add_argument("-i", "--input", help="Input FASTQ file")
    parser.add_argument("--tools", help="Comma-separated list of tools to test")
    parser.add_argument("--all", action="store_true", help="Test all available tools")
    parser.add_argument("-t", "--threads", type=int, nargs="+", default=[1, 4],
                        help="Thread counts to test (default: 1 4)")
    parser.add_argument("-r", "--runs", type=int, default=3,
                        help="Number of runs per configuration (default: 3)")
    parser.add_argument("--report", help="Output markdown report path")
    parser.add_argument("--json", help="Output JSON results path")
    parser.add_argument("-q", "--quiet", action="store_true", help="Quiet mode")
    parser.add_argument("--config", help="Path to tools.yaml config file")
    parser.add_argument("--list-tools", action="store_true", help="List available tools and exit")
    
    args = parser.parse_args()
    
    runner = BenchmarkRunner(args.config)
    
    if args.list_tools:
        print("Available tools:")
        for tid, tool in runner.tools.items():
            available = "✓" if runner._check_tool_available(tid) else "✗"
            print(f"  [{available}] {tid}: {tool.name} ({tool.category})")
        return
    
    if not args.input:
        parser.error("--input is required for benchmarking")
    
    # Determine tools to test
    if args.all:
        tool_ids = list(runner.tools.keys())
    elif args.tools:
        tool_ids = [t.strip() for t in args.tools.split(",")]
    else:
        tool_ids = ["fqc", "gzip"]  # Default
    
    # Run benchmark
    suite = runner.run_benchmark(
        input_file=args.input,
        tool_ids=tool_ids,
        threads_list=args.threads,
        runs=args.runs,
        verbose=not args.quiet
    )
    
    # Generate outputs
    if args.report:
        runner.generate_report(suite, args.report)
        print(f"\nReport saved to: {args.report}")
    
    if args.json:
        runner.export_json(suite, args.json)
        print(f"JSON saved to: {args.json}")
    
    # Print summary to stdout
    if not args.quiet and not args.report:
        print("\n" + runner.generate_report(suite))


if __name__ == "__main__":
    main()
