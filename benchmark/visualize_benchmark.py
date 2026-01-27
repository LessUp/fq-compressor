#!/usr/bin/env python3
"""
Benchmark 可视化模块
=====================
为 fq-compressor 的 benchmark 结果生成多维度可视化图表和报告。

功能：
- 生成压缩速度、解压速度、压缩率、内存使用对比图
- 支持多线程扩展性分析
- 生成 HTML 和 Markdown 格式报告
- 支持命令行独立运行或作为模块调用

使用方法：
    python visualize_benchmark.py --json results.json --output-dir docs/benchmark
"""

import argparse
import base64
import json
from io import BytesIO
from pathlib import Path
from datetime import datetime
from typing import Dict, List, Optional

try:
    import matplotlib
    matplotlib.use('Agg')  # 使用非交互式后端
    import matplotlib.pyplot as plt
    import numpy as np
    HAS_MATPLOTLIB = True
except ImportError:
    HAS_MATPLOTLIB = False
    print("Warning: matplotlib not available. Install with: pip install matplotlib")


class BenchmarkVisualizer:
    """Benchmark 结果可视化工具"""

    def __init__(self, results_json: Dict):
        """
        初始化可视化工具

        Args:
            results_json: benchmark 结果 JSON 数据
        """
        self.data = results_json
        self.results = results_json.get('results', [])
        self.figures = {}  # 存储生成的图表

    def _prepare_data(self):
        """准备绘图数据"""
        compress_data = {}
        decompress_data = {}

        for r in self.results:
            compiler = r['compiler']
            threads = r['threads']
            operation = r['operation']

            if not r['success']:
                continue

            if compiler not in compress_data:
                compress_data[compiler] = {'threads': [], 'speed': [], 'ratio': [], 'memory': []}
                decompress_data[compiler] = {'threads': [], 'speed': [], 'memory': []}

            if operation == 'compress':
                compress_data[compiler]['threads'].append(threads)
                compress_data[compiler]['speed'].append(r['input_size'] / 1024 / 1024 / r['elapsed_seconds'])
                compress_data[compiler]['ratio'].append(r['output_size'] / r['input_size'])
                compress_data[compiler]['memory'].append(r.get('peak_memory_mb', 0))
            else:  # decompress
                decompress_data[compiler]['threads'].append(threads)
                # 解压速度基于输出大小（原始数据）
                decompress_data[compiler]['speed'].append(r['output_size'] / 1024 / 1024 / r['elapsed_seconds'])
                decompress_data[compiler]['memory'].append(r.get('peak_memory_mb', 0))

        return compress_data, decompress_data

    def generate_compression_speed_chart(self) -> str:
        """生成压缩速度对比图"""
        if not HAS_MATPLOTLIB:
            return None

        compress_data, _ = self._prepare_data()

        fig, ax = plt.subplots(figsize=(10, 6))

        for compiler, data in compress_data.items():
            threads = data['threads']
            speed = data['speed']
            ax.plot(threads, speed, marker='o', label=compiler, linewidth=2, markersize=8)

        ax.set_xlabel('线程数', fontsize=12)
        ax.set_ylabel('压缩速度 (MB/s)', fontsize=12)
        ax.set_title('GCC vs Clang 压缩速度对比', fontsize=14, fontweight='bold')
        ax.legend(fontsize=11)
        ax.grid(True, alpha=0.3)
        ax.set_xticks(sorted(set(sum([d['threads'] for d in compress_data.values()], []))))

        plt.tight_layout()
        self.figures['compression_speed'] = fig
        return 'compression_speed.png'

    def generate_decompression_speed_chart(self) -> str:
        """生成解压速度对比图"""
        if not HAS_MATPLOTLIB:
            return None

        _, decompress_data = self._prepare_data()

        fig, ax = plt.subplots(figsize=(10, 6))

        for compiler, data in decompress_data.items():
            threads = data['threads']
            speed = data['speed']
            ax.plot(threads, speed, marker='s', label=compiler, linewidth=2, markersize=8)

        ax.set_xlabel('线程数', fontsize=12)
        ax.set_ylabel('解压速度 (MB/s)', fontsize=12)
        ax.set_title('GCC vs Clang 解压速度对比', fontsize=14, fontweight='bold')
        ax.legend(fontsize=11)
        ax.grid(True, alpha=0.3)
        ax.set_xticks(sorted(set(sum([d['threads'] for d in decompress_data.values()], []))))

        plt.tight_layout()
        self.figures['decompression_speed'] = fig
        return 'decompression_speed.png'

    def generate_compression_ratio_chart(self) -> str:
        """生成压缩率对比图"""
        if not HAS_MATPLOTLIB:
            return None

        compress_data, _ = self._prepare_data()

        fig, ax = plt.subplots(figsize=(10, 6))

        for compiler, data in compress_data.items():
            threads = data['threads']
            ratio = data['ratio']
            ax.plot(threads, ratio, marker='^', label=compiler, linewidth=2, markersize=8)

        ax.set_xlabel('线程数', fontsize=12)
        ax.set_ylabel('压缩率 (越小越好)', fontsize=12)
        ax.set_title('GCC vs Clang 压缩率对比', fontsize=14, fontweight='bold')
        ax.legend(fontsize=11)
        ax.grid(True, alpha=0.3)
        ax.set_xticks(sorted(set(sum([d['threads'] for d in compress_data.values()], []))))

        plt.tight_layout()
        self.figures['compression_ratio'] = fig
        return 'compression_ratio.png'

    def generate_memory_usage_chart(self) -> str:
        """生成内存使用对比图"""
        if not HAS_MATPLOTLIB:
            return None

        compress_data, decompress_data = self._prepare_data()

        fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(16, 6))

        # 压缩内存使用
        for compiler, data in compress_data.items():
            threads = data['threads']
            memory = data['memory']
            ax1.plot(threads, memory, marker='o', label=compiler, linewidth=2, markersize=8)

        ax1.set_xlabel('线程数', fontsize=12)
        ax1.set_ylabel('内存使用 (MB)', fontsize=12)
        ax1.set_title('压缩内存使用', fontsize=14, fontweight='bold')
        ax1.legend(fontsize=11)
        ax1.grid(True, alpha=0.3)
        ax1.set_xticks(sorted(set(sum([d['threads'] for d in compress_data.values()], []))))

        # 解压内存使用
        for compiler, data in decompress_data.items():
            threads = data['threads']
            memory = data['memory']
            ax2.plot(threads, memory, marker='s', label=compiler, linewidth=2, markersize=8)

        ax2.set_xlabel('线程数', fontsize=12)
        ax2.set_ylabel('内存使用 (MB)', fontsize=12)
        ax2.set_title('解压内存使用', fontsize=14, fontweight='bold')
        ax2.legend(fontsize=11)
        ax2.grid(True, alpha=0.3)
        ax2.set_xticks(sorted(set(sum([d['threads'] for d in decompress_data.values()], []))))

        plt.tight_layout()
        self.figures['memory_usage'] = fig
        return 'memory_usage.png'

    def generate_scalability_chart(self) -> str:
        """生成并行扩展性分析图"""
        if not HAS_MATPLOTLIB:
            return None

        compress_data, _ = self._prepare_data()

        fig, ax = plt.subplots(figsize=(10, 6))

        for compiler, data in compress_data.items():
            threads = np.array(data['threads'])
            speed = np.array(data['speed'])

            # 计算相对于单线程的加速比
            if threads[0] == 1:
                base_speed = speed[0]
                speedup = speed / base_speed
                ax.plot(threads, speedup, marker='o', label=f'{compiler} 实际加速比', linewidth=2, markersize=8)

        # 绘制理想线性加速比
        all_threads = sorted(set(sum([d['threads'] for d in compress_data.values()], [])))
        ax.plot(all_threads, all_threads, 'k--', label='理想线性加速', linewidth=1.5, alpha=0.7)

        ax.set_xlabel('线程数', fontsize=12)
        ax.set_ylabel('加速比 (相对于单线程)', fontsize=12)
        ax.set_title('并行扩展性分析', fontsize=14, fontweight='bold')
        ax.legend(fontsize=11)
        ax.grid(True, alpha=0.3)
        ax.set_xticks(all_threads)

        plt.tight_layout()
        self.figures['scalability'] = fig
        return 'scalability.png'

    def generate_all_charts(self) -> List[str]:
        """生成所有图表"""
        charts = []

        chart = self.generate_compression_speed_chart()
        if chart:
            charts.append(chart)

        chart = self.generate_decompression_speed_chart()
        if chart:
            charts.append(chart)

        chart = self.generate_compression_ratio_chart()
        if chart:
            charts.append(chart)

        chart = self.generate_memory_usage_chart()
        if chart:
            charts.append(chart)

        chart = self.generate_scalability_chart()
        if chart:
            charts.append(chart)

        return charts

    def save_figures(self, output_dir: Path):
        """保存所有图表到文件"""
        output_dir.mkdir(parents=True, exist_ok=True)

        for name, fig in self.figures.items():
            output_path = output_dir / f"{name}.png"
            fig.savefig(output_path, dpi=150, bbox_inches='tight')
            print(f"  保存图表: {output_path}")

    def figure_to_base64(self, fig) -> str:
        """将图表转换为 base64 编码字符串"""
        buffer = BytesIO()
        fig.savefig(buffer, format='png', dpi=150, bbox_inches='tight')
        buffer.seek(0)
        img_base64 = base64.b64encode(buffer.read()).decode()
        buffer.close()
        return img_base64

    def generate_markdown_report(self, output_dir: Path) -> str:
        """生成 Markdown 格式报告"""
        lines = [
            "# GCC vs Clang 编译器 Benchmark 报告",
            "",
            f"**生成时间:** {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}",
            "",
            "## 测试环境",
            "",
            f"- **输入文件:** `{self.data.get('input_file', 'N/A')}`",
            f"- **文件大小:** {self.data.get('input_size', 0) / 1024 / 1024:.2f} MB (压缩前)",
            f"- **测试线程数:** {', '.join(map(str, self.data.get('threads_tested', [])))}",
            "",
            "## 性能对比可视化",
            "",
        ]

        # 添加图表引用
        for name in self.figures.keys():
            chart_path = f"{name}.png"
            lines.append(f"### {self._get_chart_title(name)}")
            lines.append("")
            lines.append(f"![{name}]({chart_path})")
            lines.append("")

        # 添加数据表格
        compress_data, decompress_data = self._prepare_data()

        lines.extend([
            "## 详细数据",
            "",
            "### 压缩性能",
            "",
            "| 编译器 | 线程数 | 速度 (MB/s) | 压缩率 | 内存 (MB) |",
            "|--------|--------|-------------|--------|-----------|",
        ])

        for compiler in sorted(compress_data.keys()):
            data = compress_data[compiler]
            for i in range(len(data['threads'])):
                lines.append(
                    f"| {compiler} | {data['threads'][i]} | {data['speed'][i]:.1f} | "
                    f"{data['ratio'][i]:.4f} | {data['memory'][i]:.0f} |"
                )

        lines.extend([
            "",
            "### 解压性能",
            "",
            "| 编译器 | 线程数 | 速度 (MB/s) | 内存 (MB) |",
            "|--------|--------|-------------|-----------|",
        ])

        for compiler in sorted(decompress_data.keys()):
            data = decompress_data[compiler]
            for i in range(len(data['threads'])):
                lines.append(
                    f"| {compiler} | {data['threads'][i]} | {data['speed'][i]:.1f} | "
                    f"{data['memory'][i]:.0f} |"
                )

        # 添加分析总结
        lines.extend([
            "",
            "## 性能分析总结",
            "",
            self._generate_summary(compress_data, decompress_data),
            "",
            "---",
            "",
            "*由 fq-compressor benchmark 框架自动生成*"
        ])

        report = "\n".join(lines)

        # 保存报告
        report_path = output_dir / "benchmark_report.md"
        with open(report_path, 'w', encoding='utf-8') as f:
            f.write(report)

        print(f"  保存 Markdown 报告: {report_path}")
        return str(report_path)

    def generate_html_report(self, output_dir: Path) -> str:
        """生成 HTML 格式报告（内嵌 base64 图片）"""
        html_parts = [
            "<!DOCTYPE html>",
            "<html lang='zh-CN'>",
            "<head>",
            "    <meta charset='UTF-8'>",
            "    <meta name='viewport' content='width=device-width, initial-scale=1.0'>",
            "    <title>GCC vs Clang Benchmark 报告</title>",
            "    <style>",
            "        body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Arial, sans-serif; max-width: 1200px; margin: 40px auto; padding: 0 20px; line-height: 1.6; }",
            "        h1 { color: #2c3e50; border-bottom: 3px solid #3498db; padding-bottom: 10px; }",
            "        h2 { color: #34495e; margin-top: 30px; border-bottom: 1px solid #bdc3c7; padding-bottom: 8px; }",
            "        h3 { color: #555; }",
            "        table { border-collapse: collapse; width: 100%; margin: 20px 0; }",
            "        th, td { border: 1px solid #ddd; padding: 12px; text-align: left; }",
            "        th { background-color: #3498db; color: white; font-weight: bold; }",
            "        tr:nth-child(even) { background-color: #f9f9f9; }",
            "        .chart { margin: 30px 0; text-align: center; }",
            "        .chart img { max-width: 100%; height: auto; box-shadow: 0 2px 8px rgba(0,0,0,0.1); }",
            "        .meta { background-color: #ecf0f1; padding: 15px; border-radius: 5px; margin: 20px 0; }",
            "        .summary { background-color: #e8f5e9; padding: 20px; border-left: 4px solid #4caf50; margin: 20px 0; }",
            "        footer { margin-top: 50px; text-align: center; color: #7f8c8d; font-size: 0.9em; border-top: 1px solid #bdc3c7; padding-top: 20px; }",
            "    </style>",
            "</head>",
            "<body>",
            f"    <h1>GCC vs Clang 编译器 Benchmark 报告</h1>",
            f"    <div class='meta'>",
            f"        <strong>生成时间:</strong> {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}<br>",
            f"        <strong>输入文件:</strong> <code>{self.data.get('input_file', 'N/A')}</code><br>",
            f"        <strong>文件大小:</strong> {self.data.get('input_size', 0) / 1024 / 1024:.2f} MB<br>",
            f"        <strong>测试线程数:</strong> {', '.join(map(str, self.data.get('threads_tested', [])))}",
            f"    </div>",
            "",
            "    <h2>性能对比可视化</h2>",
        ]

        # 嵌入 base64 图表
        for name, fig in self.figures.items():
            img_base64 = self.figure_to_base64(fig)
            html_parts.append(f"    <div class='chart'>")
            html_parts.append(f"        <h3>{self._get_chart_title(name)}</h3>")
            html_parts.append(f"        <img src='data:image/png;base64,{img_base64}' alt='{name}'>")
            html_parts.append(f"    </div>")

        # 添加数据表格
        compress_data, decompress_data = self._prepare_data()

        html_parts.extend([
            "",
            "    <h2>详细数据</h2>",
            "    <h3>压缩性能</h3>",
            "    <table>",
            "        <tr><th>编译器</th><th>线程数</th><th>速度 (MB/s)</th><th>压缩率</th><th>内存 (MB)</th></tr>",
        ])

        for compiler in sorted(compress_data.keys()):
            data = compress_data[compiler]
            for i in range(len(data['threads'])):
                html_parts.append(
                    f"        <tr><td>{compiler}</td><td>{data['threads'][i]}</td>"
                    f"<td>{data['speed'][i]:.1f}</td><td>{data['ratio'][i]:.4f}</td>"
                    f"<td>{data['memory'][i]:.0f}</td></tr>"
                )

        html_parts.extend([
            "    </table>",
            "    <h3>解压性能</h3>",
            "    <table>",
            "        <tr><th>编译器</th><th>线程数</th><th>速度 (MB/s)</th><th>内存 (MB)</th></tr>",
        ])

        for compiler in sorted(decompress_data.keys()):
            data = decompress_data[compiler]
            for i in range(len(data['threads'])):
                html_parts.append(
                    f"        <tr><td>{compiler}</td><td>{data['threads'][i]}</td>"
                    f"<td>{data['speed'][i]:.1f}</td><td>{data['memory'][i]:.0f}</td></tr>"
                )

        html_parts.extend([
            "    </table>",
            "",
            "    <h2>性能分析总结</h2>",
            f"    <div class='summary'>",
            f"        {self._generate_summary(compress_data, decompress_data).replace(chr(10), '<br>')}",
            f"    </div>",
            "",
            "    <footer>",
            "        <p><em>由 fq-compressor benchmark 框架自动生成</em></p>",
            "    </footer>",
            "</body>",
            "</html>"
        ])

        html = "\n".join(html_parts)

        # 保存 HTML 报告
        html_path = output_dir / "benchmark_report.html"
        with open(html_path, 'w', encoding='utf-8') as f:
            f.write(html)

        print(f"  保存 HTML 报告: {html_path}")
        return str(html_path)

    def _get_chart_title(self, name: str) -> str:
        """获取图表标题"""
        titles = {
            'compression_speed': '压缩速度对比',
            'decompression_speed': '解压速度对比',
            'compression_ratio': '压缩率对比',
            'memory_usage': '内存使用对比',
            'scalability': '并行扩展性分析'
        }
        return titles.get(name, name)

    def _generate_summary(self, compress_data: Dict, decompress_data: Dict) -> str:
        """生成性能总结"""
        summary_parts = []

        if len(compress_data) >= 2:
            compilers = sorted(compress_data.keys())
            gcc_data = compress_data.get('GCC', compress_data.get(compilers[0]))
            clang_data = compress_data.get('Clang', compress_data.get(compilers[1]))

            # 压缩速度对比
            gcc_max_speed = max(gcc_data['speed']) if gcc_data['speed'] else 0
            clang_max_speed = max(clang_data['speed']) if clang_data['speed'] else 0

            if gcc_max_speed > 0 and clang_max_speed > 0:
                if gcc_max_speed > clang_max_speed:
                    diff = (gcc_max_speed - clang_max_speed) / clang_max_speed * 100
                    summary_parts.append(f"- **压缩速度**: GCC 比 Clang 快 **{diff:.1f}%** (最佳情况)")
                else:
                    diff = (clang_max_speed - gcc_max_speed) / gcc_max_speed * 100
                    summary_parts.append(f"- **压缩速度**: Clang 比 GCC 快 **{diff:.1f}%** (最佳情况)")

            # 压缩率对比
            gcc_avg_ratio = np.mean(gcc_data['ratio']) if gcc_data['ratio'] else 0
            clang_avg_ratio = np.mean(clang_data['ratio']) if clang_data['ratio'] else 0

            if gcc_avg_ratio > 0 and clang_avg_ratio > 0:
                if gcc_avg_ratio < clang_avg_ratio:
                    diff = (clang_avg_ratio - gcc_avg_ratio) / clang_avg_ratio * 100
                    summary_parts.append(f"- **压缩率**: GCC 比 Clang 好 **{diff:.1f}%** (平均)")
                elif clang_avg_ratio < gcc_avg_ratio:
                    diff = (gcc_avg_ratio - clang_avg_ratio) / gcc_avg_ratio * 100
                    summary_parts.append(f"- **压缩率**: Clang 比 GCC 好 **{diff:.1f}%** (平均)")
                else:
                    summary_parts.append(f"- **压缩率**: GCC 和 Clang 表现相当")

            # 并行扩展性
            if len(gcc_data['threads']) > 1:
                gcc_threads = np.array(gcc_data['threads'])
                gcc_speed = np.array(gcc_data['speed'])
                gcc_speedup = gcc_speed[-1] / gcc_speed[0] if gcc_speed[0] > 0 else 0
                gcc_efficiency = gcc_speedup / gcc_threads[-1] * 100 if gcc_threads[-1] > 0 else 0

                clang_threads = np.array(clang_data['threads'])
                clang_speed = np.array(clang_data['speed'])
                clang_speedup = clang_speed[-1] / clang_speed[0] if clang_speed[0] > 0 else 0
                clang_efficiency = clang_speedup / clang_threads[-1] * 100 if clang_threads[-1] > 0 else 0

                summary_parts.append(
                    f"- **并行效率**: GCC {gcc_efficiency:.1f}%, Clang {clang_efficiency:.1f}% "
                    f"({max(gcc_threads)} 线程相对单线程)"
                )

        if not summary_parts:
            summary_parts.append("数据不足，无法生成详细对比。")

        return "\n".join(summary_parts)


def main():
    parser = argparse.ArgumentParser(
        description="为 fq-compressor benchmark 结果生成可视化报告"
    )
    parser.add_argument("--json", required=True, help="Benchmark 结果 JSON 文件")
    parser.add_argument("--output-dir", default="docs/benchmark", help="输出目录")
    parser.add_argument("--format", choices=['png', 'markdown', 'html', 'all'], default='all',
                        help="输出格式")
    parser.add_argument("--no-matplotlib-check", action="store_true",
                        help="跳过 matplotlib 检查（用于测试）")

    args = parser.parse_args()

    if not HAS_MATPLOTLIB and not args.no_matplotlib_check:
        print("错误: matplotlib 未安装")
        print("请运行: pip install matplotlib")
        return 1

    # 加载 JSON 数据
    with open(args.json, 'r') as f:
        data = json.load(f)

    print(f"\n{'='*60}")
    print("Benchmark 可视化生成")
    print(f"{'='*60}")
    print(f"输入: {args.json}")
    print(f"输出目录: {args.output_dir}")
    print(f"{'='*60}\n")

    visualizer = BenchmarkVisualizer(data)

    # 生成图表
    print("生成图表...")
    charts = visualizer.generate_all_charts()
    print(f"  生成了 {len(charts)} 个图表")

    output_dir = Path(args.output_dir)

    # 保存图表
    if args.format in ['png', 'all']:
        print("\n保存 PNG 图表...")
        visualizer.save_figures(output_dir)

    # 生成 Markdown 报告
    if args.format in ['markdown', 'all']:
        print("\n生成 Markdown 报告...")
        visualizer.generate_markdown_report(output_dir)

    # 生成 HTML 报告
    if args.format in ['html', 'all']:
        print("\n生成 HTML 报告...")
        visualizer.generate_html_report(output_dir)

    print(f"\n{'='*60}")
    print("✓ 可视化生成完成")
    print(f"{'='*60}\n")

    return 0


if __name__ == "__main__":
    exit(main())
