#!/usr/bin/env python3
"""
Generate Markdown benchmark report from JSON results.
"""

import json
import sys
from pathlib import Path
from statistics import mean, stdev
from datetime import datetime


def load_results(json_file):
    """Load benchmark results from JSON file."""
    path = Path(json_file)
    if not path.exists():
        print(f"Error: Input file not found: {json_file}", file=sys.stderr)
        sys.exit(1)
    try:
        with open(path, 'r') as f:
            data = json.load(f)
    except json.JSONDecodeError as e:
        print(f"Error: Invalid JSON in {json_file}: {e}", file=sys.stderr)
        sys.exit(1)
    for key in ('metadata', 'results'):
        if key not in data:
            print(f"Error: Missing required key '{key}' in {json_file}", file=sys.stderr)
            sys.exit(1)
    if not data['results']:
        print(f"Warning: No benchmark results found in {json_file}", file=sys.stderr)
    return data


def calculate_statistics(results, compiler, test_type):
    """Calculate statistics for a specific compiler and test type."""
    filtered = [r for r in results if r['compiler'] == compiler and r['test'] == test_type]

    if not filtered:
        return None

    throughputs = [r['throughput_mbps'] for r in filtered]
    elapsed_times = [r['elapsed_seconds'] for r in filtered]

    stats = {
        'count': len(filtered),
        'throughput_mean': mean(throughputs),
        'throughput_stdev': stdev(throughputs) if len(throughputs) > 1 else 0,
        'throughput_min': min(throughputs),
        'throughput_max': max(throughputs),
        'elapsed_mean': mean(elapsed_times),
        'elapsed_stdev': stdev(elapsed_times) if len(elapsed_times) > 1 else 0,
    }

    # Add compression-specific stats
    if test_type == 'compression':
        ratios = [r['compression_ratio'] for r in filtered]
        stats['compression_ratio_mean'] = mean(ratios)
        stats['compression_ratio_stdev'] = stdev(ratios) if len(ratios) > 1 else 0

    return stats


def generate_report(data, output_file):
    """Generate Markdown report."""
    metadata = data['metadata']
    results = data['results']

    # Get unique compilers
    compilers = sorted(set(r['compiler'] for r in results))

    # Calculate statistics
    stats = {}
    for compiler in compilers:
        stats[compiler] = {
            'compression': calculate_statistics(results, compiler, 'compression'),
            'decompression': calculate_statistics(results, compiler, 'decompression'),
        }

    # Generate report
    report = []

    # Header
    report.append("# fq-compressor Benchmark Report")
    report.append("")
    report.append(f"**Generated**: {metadata['timestamp']}")
    report.append(f"**Hostname**: {metadata['hostname']}")
    report.append(f"**CPU**: {metadata['cpu']}")
    report.append(f"**Cores**: {metadata['cores']}")
    report.append(f"**Iterations**: {metadata['iterations']}")
    report.append("")

    # Test files
    report.append("## Test Data")
    report.append("")
    for test_file in metadata['test_files']:
        report.append(f"- `{test_file}`")
    report.append("")

    # Summary table
    report.append("## Performance Summary")
    report.append("")
    report.append("| Compiler | Test | Throughput (MB/s) | Std Dev | Min | Max | Time (s) |")
    report.append("|----------|------|-------------------|---------|-----|-----|----------|")

    for compiler in compilers:
        for test_type in ['compression', 'decompression']:
            s = stats[compiler][test_type]
            if s:
                report.append(
                    f"| {compiler.upper()} | {test_type.capitalize()} | "
                    f"{s['throughput_mean']:.2f} | {s['throughput_stdev']:.2f} | "
                    f"{s['throughput_min']:.2f} | {s['throughput_max']:.2f} | "
                    f"{s['elapsed_mean']:.3f} |"
                )

    report.append("")

    # Compression ratio
    report.append("## Compression Ratio")
    report.append("")
    report.append("| Compiler | Ratio | Std Dev |")
    report.append("|----------|-------|---------|")

    for compiler in compilers:
        s = stats[compiler]['compression']
        if s:
            report.append(
                f"| {compiler.upper()} | {s['compression_ratio_mean']:.4f}x | "
                f"{s['compression_ratio_stdev']:.4f} |"
            )

    report.append("")

    # Compiler comparison
    if len(compilers) == 2:
        report.append("## Compiler Comparison")
        report.append("")

        gcc_comp = stats['gcc']['compression']
        clang_comp = stats['clang']['compression']
        gcc_decomp = stats['gcc']['decompression']
        clang_decomp = stats['clang']['decompression']

        if gcc_comp and clang_comp:
            comp_speedup = clang_comp['throughput_mean'] / gcc_comp['throughput_mean']
            report.append(f"**Compression**: Clang is {comp_speedup:.2f}x {'faster' if comp_speedup > 1 else 'slower'} than GCC")

        if gcc_decomp and clang_decomp:
            decomp_speedup = clang_decomp['throughput_mean'] / gcc_decomp['throughput_mean']
            report.append(f"**Decompression**: Clang is {decomp_speedup:.2f}x {'faster' if decomp_speedup > 1 else 'slower'} than GCC")

        report.append("")

    # Detailed results
    report.append("## Detailed Results")
    report.append("")

    for compiler in compilers:
        report.append(f"### {compiler.upper()}")
        report.append("")

        # Compression
        report.append("#### Compression")
        report.append("")
        report.append("| Iteration | Input Size | Output Size | Ratio | Time (s) | Throughput (MB/s) |")
        report.append("|-----------|------------|-------------|-------|----------|-------------------|")

        comp_results = [r for r in results if r['compiler'] == compiler and r['test'] == 'compression']
        for r in comp_results:
            report.append(
                f"| {r['iteration']} | {r['input_size']:,} | {r['output_size']:,} | "
                f"{r['compression_ratio']:.4f}x | {r['elapsed_seconds']:.3f} | "
                f"{r['throughput_mbps']:.2f} |"
            )

        report.append("")

        # Decompression
        report.append("#### Decompression")
        report.append("")
        report.append("| Iteration | Input Size | Output Size | Time (s) | Throughput (MB/s) |")
        report.append("|-----------|------------|-------------|----------|-------------------|")

        decomp_results = [r for r in results if r['compiler'] == compiler and r['test'] == 'decompression']
        for r in decomp_results:
            report.append(
                f"| {r['iteration']} | {r['input_size']:,} | {r['output_size']:,} | "
                f"{r['elapsed_seconds']:.3f} | {r['throughput_mbps']:.2f} |"
            )

        report.append("")

    # Recommendations
    report.append("## Recommendations")
    report.append("")

    if len(compilers) == 2:
        gcc_comp = stats['gcc']['compression']
        clang_comp = stats['clang']['compression']

        if gcc_comp and clang_comp:
            if clang_comp['throughput_mean'] > gcc_comp['throughput_mean'] * 1.1:
                report.append("- **Use Clang** for better compression performance")
            elif gcc_comp['throughput_mean'] > clang_comp['throughput_mean'] * 1.1:
                report.append("- **Use GCC** for better compression performance")
            else:
                report.append("- Both compilers show similar performance")

    report.append("")
    report.append("---")
    report.append("")
    report.append(f"*Report generated on {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}*")

    # Write report
    with open(output_file, 'w') as f:
        f.write('\n'.join(report))


def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <input.json> <output.md>", file=sys.stderr)
        sys.exit(1)

    input_file = sys.argv[1]
    output_file = sys.argv[2]

    data = load_results(input_file)

    if not data['results']:
        print("No results to report. Skipping report generation.")
        return

    try:
        generate_report(data, output_file)
    except (KeyError, TypeError) as e:
        print(f"Error generating report: {e}", file=sys.stderr)
        sys.exit(1)

    print(f"Report generated: {output_file}")


if __name__ == '__main__':
    main()
