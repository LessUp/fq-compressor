#!/usr/bin/env python3
"""
Generate HTML visualization charts from benchmark JSON results.
"""

import json
import sys
from pathlib import Path
from statistics import mean


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


def generate_html(data, output_file):
    """Generate HTML with interactive charts."""
    metadata = data['metadata']
    results = data['results']

    # Get unique compilers
    compilers = sorted(set(r['compiler'] for r in results))

    # Prepare data for charts
    compression_data = {}
    decompression_data = {}

    for compiler in compilers:
        comp_results = [r for r in results if r['compiler'] == compiler and r['test'] == 'compression']
        decomp_results = [r for r in results if r['compiler'] == compiler and r['test'] == 'decompression']

        compression_data[compiler] = {
            'throughputs': [r['throughput_mbps'] for r in comp_results],
            'ratios': [r['compression_ratio'] for r in comp_results],
            'times': [r['elapsed_seconds'] for r in comp_results],
        }

        decompression_data[compiler] = {
            'throughputs': [r['throughput_mbps'] for r in decomp_results],
            'times': [r['elapsed_seconds'] for r in decomp_results],
        }

    # Generate HTML
    html = f"""<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>fq-compressor Benchmark Results</title>
    <script src="https://cdn.plot.ly/plotly-2.27.0.min.js"></script>
    <style>
        body {{
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, Cantarell, sans-serif;
            max-width: 1400px;
            margin: 0 auto;
            padding: 20px;
            background-color: #f5f5f5;
        }}
        .header {{
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 30px;
            border-radius: 10px;
            margin-bottom: 30px;
            box-shadow: 0 4px 6px rgba(0,0,0,0.1);
        }}
        .header h1 {{
            margin: 0 0 10px 0;
            font-size: 2.5em;
        }}
        .metadata {{
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 10px;
            margin-top: 20px;
        }}
        .metadata-item {{
            background: rgba(255,255,255,0.1);
            padding: 10px;
            border-radius: 5px;
        }}
        .metadata-label {{
            font-size: 0.9em;
            opacity: 0.8;
        }}
        .metadata-value {{
            font-size: 1.2em;
            font-weight: bold;
            margin-top: 5px;
        }}
        .chart-container {{
            background: white;
            padding: 20px;
            border-radius: 10px;
            margin-bottom: 30px;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        }}
        .chart-title {{
            font-size: 1.5em;
            font-weight: bold;
            margin-bottom: 15px;
            color: #333;
        }}
        .summary-grid {{
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
            gap: 20px;
            margin-bottom: 30px;
        }}
        .summary-card {{
            background: white;
            padding: 20px;
            border-radius: 10px;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        }}
        .summary-card h3 {{
            margin: 0 0 15px 0;
            color: #667eea;
            font-size: 1.2em;
        }}
        .metric {{
            display: flex;
            justify-content: space-between;
            padding: 8px 0;
            border-bottom: 1px solid #eee;
        }}
        .metric:last-child {{
            border-bottom: none;
        }}
        .metric-label {{
            color: #666;
        }}
        .metric-value {{
            font-weight: bold;
            color: #333;
        }}
        .winner {{
            background: linear-gradient(135deg, #11998e 0%, #38ef7d 100%);
            color: white;
        }}
        .winner h3 {{
            color: white;
        }}
        .winner .metric-label {{
            color: rgba(255,255,255,0.8);
        }}
        .winner .metric-value {{
            color: white;
        }}
        .footer {{
            text-align: center;
            color: #666;
            margin-top: 40px;
            padding: 20px;
        }}
    </style>
</head>
<body>
    <div class="header">
        <h1>🚀 fq-compressor Benchmark Results</h1>
        <p>Comprehensive performance analysis of GCC vs Clang builds</p>
        <div class="metadata">
            <div class="metadata-item">
                <div class="metadata-label">Timestamp</div>
                <div class="metadata-value">{metadata['timestamp']}</div>
            </div>
            <div class="metadata-item">
                <div class="metadata-label">CPU</div>
                <div class="metadata-value">{metadata['cpu']}</div>
            </div>
            <div class="metadata-item">
                <div class="metadata-label">Cores</div>
                <div class="metadata-value">{metadata['cores']}</div>
            </div>
            <div class="metadata-item">
                <div class="metadata-label">Iterations</div>
                <div class="metadata-value">{metadata['iterations']}</div>
            </div>
        </div>
    </div>

    <div class="summary-grid">
"""

    # Calculate summary statistics
    for compiler in compilers:
        comp_throughput = mean(compression_data[compiler]['throughputs'])
        decomp_throughput = mean(decompression_data[compiler]['throughputs'])
        comp_ratio = mean(compression_data[compiler]['ratios'])

        html += f"""
        <div class="summary-card">
            <h3>{compiler.upper()} Performance</h3>
            <div class="metric">
                <span class="metric-label">Compression Speed</span>
                <span class="metric-value">{comp_throughput:.2f} MB/s</span>
            </div>
            <div class="metric">
                <span class="metric-label">Decompression Speed</span>
                <span class="metric-value">{decomp_throughput:.2f} MB/s</span>
            </div>
            <div class="metric">
                <span class="metric-label">Compression Ratio</span>
                <span class="metric-value">{comp_ratio:.4f}x</span>
            </div>
        </div>
"""

    # Winner card
    if len(compilers) == 2:
        gcc_comp = mean(compression_data['gcc']['throughputs'])
        clang_comp = mean(compression_data['clang']['throughputs'])
        winner = 'clang' if clang_comp > gcc_comp else 'gcc'
        speedup = max(gcc_comp, clang_comp) / min(gcc_comp, clang_comp)

        html += f"""
        <div class="summary-card winner">
            <h3>🏆 Winner: {winner.upper()}</h3>
            <div class="metric">
                <span class="metric-label">Speedup</span>
                <span class="metric-value">{speedup:.2f}x faster</span>
            </div>
            <div class="metric">
                <span class="metric-label">Advantage</span>
                <span class="metric-value">{(speedup - 1) * 100:.1f}% improvement</span>
            </div>
        </div>
"""

    html += """
    </div>

    <div class="chart-container">
        <div class="chart-title">📊 Compression Throughput Comparison</div>
        <div id="compression-throughput"></div>
    </div>

    <div class="chart-container">
        <div class="chart-title">📊 Decompression Throughput Comparison</div>
        <div id="decompression-throughput"></div>
    </div>

    <div class="chart-container">
        <div class="chart-title">📊 Compression Ratio</div>
        <div id="compression-ratio"></div>
    </div>

    <div class="chart-container">
        <div class="chart-title">📊 Execution Time Comparison</div>
        <div id="execution-time"></div>
    </div>

    <div class="footer">
        <p>Generated by fq-compressor benchmark suite</p>
        <p>For more details, see the full benchmark report</p>
    </div>

    <script>
"""

    # Generate Plotly charts
    # Compression throughput
    html += """
        var compressionThroughputData = [
"""
    for compiler in compilers:
        iterations = list(range(1, len(compression_data[compiler]['throughputs']) + 1))
        html += f"""
            {{
                x: {iterations},
                y: {compression_data[compiler]['throughputs']},
                name: '{compiler.upper()}',
                type: 'scatter',
                mode: 'lines+markers',
                marker: {{ size: 10 }}
            }},
"""
    html += """
        ];
        var compressionThroughputLayout = {
            xaxis: { title: 'Iteration' },
            yaxis: { title: 'Throughput (MB/s)' },
            hovermode: 'closest'
        };
        Plotly.newPlot('compression-throughput', compressionThroughputData, compressionThroughputLayout);
"""

    # Decompression throughput
    html += """
        var decompressionThroughputData = [
"""
    for compiler in compilers:
        iterations = list(range(1, len(decompression_data[compiler]['throughputs']) + 1))
        html += f"""
            {{
                x: {iterations},
                y: {decompression_data[compiler]['throughputs']},
                name: '{compiler.upper()}',
                type: 'scatter',
                mode: 'lines+markers',
                marker: {{ size: 10 }}
            }},
"""
    html += """
        ];
        var decompressionThroughputLayout = {
            xaxis: { title: 'Iteration' },
            yaxis: { title: 'Throughput (MB/s)' },
            hovermode: 'closest'
        };
        Plotly.newPlot('decompression-throughput', decompressionThroughputData, decompressionThroughputLayout);
"""

    # Compression ratio
    html += """
        var compressionRatioData = [
"""
    for compiler in compilers:
        iterations = list(range(1, len(compression_data[compiler]['ratios']) + 1))
        html += f"""
            {{
                x: {iterations},
                y: {compression_data[compiler]['ratios']},
                name: '{compiler.upper()}',
                type: 'bar'
            }},
"""
    html += """
        ];
        var compressionRatioLayout = {
            xaxis: { title: 'Iteration' },
            yaxis: { title: 'Compression Ratio' },
            barmode: 'group'
        };
        Plotly.newPlot('compression-ratio', compressionRatioData, compressionRatioLayout);
"""

    # Execution time
    html += """
        var executionTimeData = [
"""
    for compiler in compilers:
        html += f"""
            {{
                x: ['Compression', 'Decompression'],
                y: [{mean(compression_data[compiler]['times']):.3f}, {mean(decompression_data[compiler]['times']):.3f}],
                name: '{compiler.upper()}',
                type: 'bar'
            }},
"""
    html += """
        ];
        var executionTimeLayout = {
            xaxis: { title: 'Operation' },
            yaxis: { title: 'Time (seconds)' },
            barmode: 'group'
        };
        Plotly.newPlot('execution-time', executionTimeData, executionTimeLayout);
    </script>
</body>
</html>
"""

    # Write HTML
    with open(output_file, 'w') as f:
        f.write(html)


def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <input.json> <output.html>", file=sys.stderr)
        sys.exit(1)

    input_file = sys.argv[1]
    output_file = sys.argv[2]

    data = load_results(input_file)

    if not data['results']:
        print("No results to visualize. Skipping chart generation.")
        return

    try:
        generate_html(data, output_file)
    except (KeyError, TypeError) as e:
        print(f"Error generating charts: {e}", file=sys.stderr)
        sys.exit(1)

    print(f"HTML charts generated: {output_file}")


if __name__ == '__main__':
    main()
