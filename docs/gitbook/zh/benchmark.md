# Benchmark

## 关注指标

fq-compressor 的 benchmark 重点关注：

- 压缩速度
- 解压速度
- 压缩率
- bits per base
- 峰值内存
- 多线程扩展性

## 主要脚本

- `benchmark/compiler_benchmark.py`
- `benchmark/benchmark.py`
- `benchmark/visualize_benchmark.py`
- `scripts/benchmark.sh`

## 结果位置

- `docs/benchmark/results/report-latest.md`
- `docs/benchmark/results/charts-latest.html`

## 建议阅读顺序

1. 先看 `docs/BENCHMARK.md`
2. 再看 `docs/benchmark-summary.md`
3. 最后查看 `docs/benchmark/results/` 中的最新结果
