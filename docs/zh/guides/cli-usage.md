# CLI 使用

这一页只保留操作层面的命令集合。如果你想理解背后的实现边界，请读 [架构](/zh/architecture/)；如果你想核对仓库出处，请看 [资源](/zh/resources/)。

## 全局选项

这些选项写在子命令之前：

- `-t, --threads <n>`：设置线程数（`0` 表示自动）
- `-v, --verbose`：提高日志详细度
- `-q, --quiet`：仅输出错误
- `--memory-limit <MB>`：限制内存预算
- `--no-progress`：关闭进度显示
- `--log-file <path>` / `--log-level <level>`：调整日志输出
- `--json`：为 `info` 和 `verify` 输出 JSON
- `-V, --version`：打印版本号

## compress

```bash
fqc compress -i reads.fastq -o reads.fqc
```

常用选项：

- `-2, --input2 <path>`：paired-end 第二个 FASTQ
- `--paired`：启用 paired-end 模式
- `--interleaved`：单文件交错 paired-end 输入
- `-l, --level <1-9>`：压缩级别
- `--preserve-order` 或 `--no-reorder`：关闭全局重排
- `--streaming`：面向 stdin 的流式模式，压缩率空间更小
- `--quality-mode <lossless|illumina8|discard>`
- `--id-mode <exact|tokenize|discard>`
- `--long-read-mode <auto|short|medium|long>`
- `--block-reads <n>` / `--max-block-bases <n>`：块大小控制
- `-f, --force`：覆盖已有输出文件

示例：

```bash
fqc compress -i reads.fastq.gz -o reads.fqc -t 8
fqc compress -i r1.fastq -2 r2.fastq -o paired.fqc --paired
fqc compress -i - -o stream.fqc --streaming
```

## decompress

```bash
fqc decompress -i reads.fqc -o restored.fastq
```

常用选项：

- `--range <start:end>`：提取 reads 区间
- `--range-pairs <start:end>`：按 read pair 提取区间
- `--original-order`：在存在 reorder map 时恢复原始顺序
- `--header-only`：只输出 read ID
- `--streams <all|id|seq|qual|...>`：只解码部分流
- `--output-format <fastq|fasta|tsv|raw>`
- `--split-pe`：把 paired-end 输出拆分
- `--skip-corrupted`：遇到损坏块时继续
- `--verify` / `--no-verify`：控制解压时是否校验 checksum

示例：

```bash
fqc decompress -i reads.fqc --range 1:1000 -o subset.fastq
fqc decompress -i paired.fqc -o restored.fastq --original-order
fqc decompress -i archive.fqc -o headers.txt --header-only
```

## inspect 与 verify

```bash
fqc info -i reads.fqc
fqc verify -i reads.fqc
```

常用选项：

- `fqc info --json --detailed --show-index --show-codecs`
- `fqc verify --mode quick`
- `fqc verify --fail-fast --json`

## 操作者常见流程

```bash
fqc compress -i reads.fastq -o reads.fqc -t 8
fqc verify -i reads.fqc
fqc info -i reads.fqc --show-index
fqc decompress -i reads.fqc --range 1001:2000 -o spot-check.fastq
```

## 继续阅读

- [快速开始](./getting-started)：完成第一次验证运行
- [贡献](./contributing)：查看仓库检查与文档更新要求
