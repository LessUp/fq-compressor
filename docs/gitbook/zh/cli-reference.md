# 命令行参考

fq-compressor 提供四个主要命令：`compress`、`decompress`、`info` 和 `verify`。

## 全局选项

| 选项 | 说明 |
|------|------|
| `-h, --help` | 显示帮助信息 |
| `--version` | 显示版本信息 |
| `-v, --verbose` | 启用详细日志 |

## compress

将 FASTQ 文件压缩为 `.fqc` 归档格式。

```bash
fqc compress [选项] -i <输入> -o <输出>
```

### 选项

| 选项 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `-i, --input` | 路径 | *必填* | 输入 FASTQ 文件（`.fastq`、`.fq`、`.fastq.gz`、`.fq.gz`） |
| `-o, --output` | 路径 | *必填* | 输出 `.fqc` 归档文件 |
| `-I, --input2` | 路径 | — | 双端数据的第二个输入文件 |
| `-t, --threads` | 整数 | CPU 核心数 | 并行线程数 |
| `-l, --level` | 整数 | 5 | 压缩级别（1=快速，9=最优） |
| `--memory-limit` | 整数 | 8192 | 内存预算（MB） |
| `--pipeline` | 标志 | 关闭 | 启用 3 阶段并行流水线 |
| `--no-reorder` | 标志 | 关闭 | 禁用 read 重排序（保持原始顺序） |

### 示例

```bash
# 基础压缩
fqc compress -i reads.fastq -o reads.fqc

# 高压缩比 + 4 线程
fqc compress -i reads.fastq -o reads.fqc -l 9 -t 4

# 双端压缩
fqc compress -i R1.fastq.gz -I R2.fastq.gz -o paired.fqc

# 流水线模式 + 内存限制
fqc compress -i large.fastq -o large.fqc --pipeline --memory-limit 16384
```

## decompress

将 `.fqc` 归档解压回 FASTQ 格式。

```bash
fqc decompress [选项] -i <输入> -o <输出>
```

### 选项

| 选项 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `-i, --input` | 路径 | *必填* | 输入 `.fqc` 归档文件 |
| `-o, --output` | 路径 | *必填* | 输出 FASTQ 文件 |
| `-O, --output2` | 路径 | — | 双端数据的第二个输出文件 |
| `-t, --threads` | 整数 | CPU 核心数 | 并行线程数 |
| `--range` | 字符串 | — | 提取指定 read 范围（如 `1000:2000`） |
| `--pipeline` | 标志 | 关闭 | 启用 3 阶段并行流水线 |

### 示例

```bash
# 基础解压
fqc decompress -i reads.fqc -o reads.fastq

# 随机访问：提取第 500-1500 条 reads
fqc decompress -i reads.fqc --range 500:1500 -o subset.fastq

# 双端解压
fqc decompress -i paired.fqc -o R1.fastq -O R2.fastq
```

## info

显示 `.fqc` 归档的元数据和统计信息。

```bash
fqc info [选项] <输入>
```

### 输出字段

| 字段 | 说明 |
|------|------|
| Read 条数 | 归档中的总 read 数量 |
| 原始大小 | 未压缩的字节数 |
| 压缩大小 | 归档的字节数 |
| 压缩比 | 原始 / 压缩 |
| Bits per base | 每个 DNA 碱基的平均压缩位数 |
| Block 数量 | 独立 block 的数量 |
| Read 长度 | 最小 / 最大 / 中位数 |

## verify

验证 `.fqc` 归档的完整性。

```bash
fqc verify [选项] <输入>
```

执行全面的完整性验证：
- 文件头魔数和版本检查
- Block 级别 CRC32 验证
- 索引一致性检查
- 文件尾校验和验证

### 退出码

| 代码 | 含义 |
|------|------|
| 0 | 成功 |
| 1 | I/O 错误 |
| 2 | 格式错误 |
| 3 | 压缩错误 |
| 4 | 验证错误 |
| 5 | 内部错误 |
