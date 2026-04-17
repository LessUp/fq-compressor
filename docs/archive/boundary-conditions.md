# fq-compressor 边界条件处理规范

> 文档版本: 1.0
> 更新日期: 2026-01-27
> 覆盖范围: 基本容错 (P2级别)

---

## 1. 输入边界条件

### 1.1 空文件处理

#### 空FASTQ文件 (0 reads)

**输入**: 0字节或只包含空行/注释的文件

**行为**:
```
compress:
  - 生成有效的空FQC文件
  - 包含GlobalHeader (totalReads=0, totalBlocks=0)
  - 包含空的Block Index
  - 包含Footer (checksum of empty data)

decompress:
  - 读取空FQC文件
  - 生成空FASTQ文件 (0字节)
  - 不报错
```

**验证**:
```bash
# 创建空文件
touch empty.fq

# 压缩
./fqc compress -i empty.fq -o empty.fqc

# 验证: empty.fqc应包含合法header+footer
./fqc info empty.fqc
# 应显示: Total reads: 0, Blocks: 0

# 解压
./fqc decompress -i empty.fqc -o empty.out.fq

# 验证: 应生成0字节文件
test -s empty.out.fq && echo "FAIL" || echo "PASS"
```

---

#### 单条Read文件

**输入**: 1条有效FASTQ record (4行)

**行为**:
```
compress:
  - 生成单块FQC文件 (totalBlocks=1)
  - 跳过重排序 (无需优化)
  - 正常压缩序列、质量、ID

decompress:
  - 正常还原单条read
```

**验证**:
```bash
# 创建单read文件
cat > single.fq <<EOF
@read_0001
ACGT
+
IIII
EOF

# 往返测试
./fqc compress -i single.fq -o single.fqc
./fqc decompress -i single.fqc -o single.out.fq
diff single.fq single.out.fq  # 应相同
```

---

### 1.2 Read长度边界

#### 极短Read (< 10bp)

**输入**: 如 `ACGT` (4bp)

**行为**:
```
compress:
  - 分类为 SHORT read
  - 使用ABC压缩策略
  - 正常编码,无特殊处理
```

---

#### 边界Read (511bp)

**输入**: 恰好511bp的read

**行为**:
```
compress:
  - 分类为 SHORT read (max <= 511)
  - 使用ABC + 重排序
```

**临界点**: 512bp及以上切换到MEDIUM策略

---

#### 极长Read (> 100KB)

**输入**: 如100,001bp的long read

**行为**:
```
compress:
  - 分类为 ULTRA-LONG read
  - 使用Zstd压缩
  - 禁用重排序
  - 限制块大小 (max_block_bases)
```

---

### 1.3 质量值边界

#### 缺失质量值

**输入**: FASTQ文件质量行为空或全`!` (Phred+33最低值)

**行为 (质量模式: lossless)**:
```
compress:
  - 正常存储空质量值或低质量值
  - 不报错

decompress:
  - 完整还原
```

**行为 (质量模式: discard)**:
```
compress:
  - 丢弃质量值,设置标志位

decompress:
  - 生成占位符质量值 (默认'!' Phred+33 = 0)
  - 通过--placeholder-qual可自定义
```

---

#### 质量值超范围

**输入**: 质量字符 < 33 或 > 126 (非ASCII可打印字符)

**行为**:
```
compress:
  - FastqParser 验证时检测
  - 抛出 FormatError 异常
  - 错误消息: "Invalid quality score character: [value] at line [N]"
```

**边界允许**: Phred+33范围 [33, 126] → Phred quality [0, 93]

---

### 1.4 ID格式边界

#### 非法字符

**输入**: ID包含不可打印字符、换行符

**行为**:
```
compress:
  - FastqParser 检测非法字符
  - 抛出 FormatError
  - 错误消息: "Invalid character in read ID at line [N]"
```

---

#### 超长ID (> 1KB)

**输入**: ID长度超过1024字符

**行为**:
```
compress:
  - 正常压缩,无长度限制
  - IDCompressor Delta + Tokenization 处理
  - 性能可能降低 (日志警告)
```

**优化**: 如检测到大量超长ID,可发出警告

---

#### 空ID

**输入**: `@\n` (ID为空)

**行为**:
```
compress:
  - FastqParser 检测到空ID
  - 抛出 FormatError
  - 错误消息: "Empty read ID at line [N]"
```

---

### 1.5 序列边界

#### 包含N碱基

**输入**: `ACGTNNNACGT`

**行为**:
```
compress:
  - 正常编码,'N'碱基按4进制编码 (N=00, A=01, C=10, G=11, T=11)
  - Noise encoding 会记录N的位置
```

---

#### 包含非ACGTN字符

**输入**: `ACGTX`, `acgt` (小写)

**行为**:
```
compress:
  - FastqParser 验证阶段检测
  - 抛出 FormatError
  - 错误消息: "Invalid base character 'X' at line [N]"
```

**允许字符**: 仅 A, C, G, T, N (大写)

---

## 2. 文件格式边界

### 2.1 损坏的FQC文件

#### 魔数错误

**输入**: FQC文件前8字节不是 `\x89FQC\r\n\x1a\n`

**行为**:
```
decompress:
  - FQCReader::open() 检测
  - 抛出 FormatError
  - 错误消息: "Invalid .fqc magic header"
  - 退出码: 3 (FORMAT_ERROR)
```

---

#### 校验和错误

**输入**: FQC文件footer中的checksum与实际内容不匹配

**行为 (默认)**:
```
decompress:
  - FQCReader 验证checksum
  - 抛出 ChecksumError
  - 错误消息: "Checksum mismatch: expected [hex], got [hex]"
  - 退出码: 4 (CHECKSUM_ERROR)
```

**行为 (--skip-corrupted 模式)**:
```
decompress:
  - 跳过损坏的块
  - 输出损坏块的占位符 (空序列或用户指定)
  - 日志警告: "Skipping corrupted block [id]"
  - 继续处理下一个块
  - 退出码: 0 (成功,但有警告)
```

---

#### 截断文件 (不完整)

**输入**: FQC文件突然结束,缺少footer

**行为**:
```
decompress:
  - FQCReader 读取时遇到EOF
  - 抛出 IOError
  - 错误消息: "Unexpected EOF: incomplete .fqc file"
  - 退出码: 2 (IO_ERROR)
```

---

### 2.2 版本不兼容

#### 未来版本 (major version > 当前)

**输入**: FQC文件header中 `version = 2.0`, 当前工具 `version = 1.x`

**行为**:
```
decompress:
  - FQCReader::readHeader() 检测
  - 抛出 FormatError
  - 错误消息: "Unsupported FQC version 2.0 (this tool supports 1.x)"
  - 退出码: 3 (FORMAT_ERROR)
```

---

#### 向后兼容 (minor version 不同)

**输入**: FQC文件 `version = 1.1`, 工具 `version = 1.0`

**行为**:
```
decompress:
  - 正常读取
  - 跳过未知字段 (通过headerSize计算)
  - 日志信息: "Reading FQC v1.1 with v1.0 reader (forward compatible)"
```

---

## 3. 内存边界

### 3.1 内存不足

#### 无法分配单个块

**场景**: 内存限制设置过小 (如 `--memory-limit 100MB`),但单个块需要200MB

**行为**:
```
compress:
  - MemoryBudget::allocate() 失败
  - 抛出 MemoryError
  - 错误消息: "Insufficient memory: need [X]MB, available [Y]MB"
  - 建议: "Try increasing --memory-limit or reducing --block-size"
  - 退出码: 5 (MEMORY_ERROR)
```

---

#### 系统内存耗尽

**场景**: 系统物理内存耗尽

**行为**:
```
compress:
  - std::bad_alloc 异常
  - 捕获后转换为 MemoryError
  - 清理已分配资源
  - 退出码: 5
```

---

### 3.2 块大小异常

#### 块太小 (< 100 reads)

**输入**: `--block-size 50`

**行为**:
```
compress:
  - validateOptions() 检测
  - 发出警告: "Block size 50 is very small, compression ratio may be poor"
  - 继续执行 (不报错)
```

---

#### 块太大 (> 10M reads)

**输入**: `--block-size 20000000`

**行为**:
```
compress:
  - validateOptions() 检测
  - 发出警告: "Block size 20M is very large, may cause memory issues"
  - 调整为最大值 1M reads
  - 日志信息: "Block size capped at 1M reads"
```

---

## 4. 并发边界

### 4.1 线程数边界

#### 线程数 < 1

**输入**: `--threads 0` 或 `--threads -1`

**行为**:
```
compress:
  - validateOptions() 检测
  - 抛出 ArgumentError
  - 错误消息: "Thread count must be >= 1"
  - 退出码: 1 (ARGUMENT_ERROR)
```

---

#### 线程数 > CPU核心数

**输入**: `--threads 64` (系统只有8核)

**行为**:
```
compress:
  - validateOptions() 检测
  - 发出警告: "Thread count 64 exceeds CPU cores (8), may reduce performance"
  - 继续执行 (不限制)
```

---

## 5. stdin/stdout 边界

### 5.1 stdin 模式

#### 无法采样

**场景**: `cat data.fq | fqc compress -i - -o out.fqc`

**行为**:
```
compress:
  - detectReadLengthClass() 检测stdin
  - 无法回退采样
  - 使用默认 MEDIUM 策略
  - 日志警告: "Cannot sample stdin for length detection, using MEDIUM strategy"
```

---

### 5.2 stdout 模式

#### 输出二进制到终端

**场景**: `fqc compress -i input.fq -o -` (stdout非管道)

**行为**:
```
compress:
  - validateOptions() 检测 isatty(stdout)
  - 发出警告: "Writing binary data to terminal may cause display issues"
  - 继续执行 (不阻止)
```

---

## 6. 文件系统边界

### 6.1 磁盘空间不足

**场景**: 压缩过程中磁盘满

**行为**:
```
compress:
  - FQCWriter::write() 写入失败
  - 捕获 std::ios::failure
  - 抛出 IOError
  - 清理临时文件
  - 错误消息: "Disk full or write error: [errno]"
  - 退出码: 2 (IO_ERROR)
```

---

### 6.2 文件权限错误

**场景**: 输出目录无写权限

**行为**:
```
compress:
  - validateOptions() 阶段检测
  - 尝试创建临时文件
  - 失败时抛出 IOError
  - 错误消息: "Cannot write to [path]: Permission denied"
  - 退出码: 2
```

---

## 7. Paired-End 边界

### 7.1 R1/R2 不配对

**场景**: PE压缩时,R1有100K reads,R2有99K reads

**行为**:
```
compress --paired:
  - 读取R1和R2文件
  - 检测读取数量不匹配
  - 抛出 FormatError
  - 错误消息: "PE read count mismatch: R1=[100000], R2=[99000]"
  - 退出码: 3 (FORMAT_ERROR)
```

---

## 8. 压缩级别边界

### 8.1 超出范围

**输入**: `--level 0` 或 `--level 10`

**行为**:
```
compress:
  - validateOptions() 检测
  - 抛出 ArgumentError
  - 错误消息: "Compression level must be 1-9"
  - 退出码: 1
```

---

## 9. 退出码规范

| 退出码 | 名称 | 触发条件 | 示例 |
|--------|------|----------|------|
| 0 | SUCCESS | 成功完成 | 正常压缩/解压 |
| 1 | ARGUMENT_ERROR | 命令行参数错误 | --level 0 |
| 2 | IO_ERROR | 文件I/O错误 | 文件不存在,磁盘满 |
| 3 | FORMAT_ERROR | 格式错误 | 损坏的FQC文件,非法FASTQ |
| 4 | CHECKSUM_ERROR | 校验和错误 | FQC checksum不匹配 |
| 5 | MEMORY_ERROR | 内存不足 | 无法分配块内存 |

---

## 10. 测试覆盖清单

### 必须测试的边界case

- [ ] 空FASTQ文件 (0 reads)
- [ ] 单read文件 (1 read)
- [ ] 极短read (4bp)
- [ ] 边界read (511bp vs 512bp)
- [ ] 质量值缺失
- [ ] 非法字符 (序列, 质量, ID)
- [ ] 损坏的FQC魔数
- [ ] 损坏的FQC校验和
- [ ] 截断的FQC文件
- [ ] 内存不足 (通过限制--memory-limit)
- [ ] 线程数边界 (0, 负数, 超大)
- [ ] stdin无法采样
- [ ] 磁盘空间不足 (模拟)
- [ ] 文件权限错误

---

## 11. 错误消息规范

### 格式

```
ERROR: [简短描述]
Location: [文件:行号] (可选)
Reason: [详细原因]
Suggestion: [修复建议] (可选)
```

### 示例

```
ERROR: Invalid .fqc magic header
Location: /path/to/file.fqc:1
Reason: Expected '\x89FQC\r\n\x1a\n', got '\x1f\x8b\x08...' (looks like gzip)
Suggestion: This file appears to be gzip-compressed, use gunzip first
```

---

## 12. 实施优先级

### P0 - MVP必需

- ✅ 空文件处理
- ✅ 损坏FQC魔数检测
- ✅ 基本参数验证 (线程数, 压缩级别)

### P1 - 生产就绪

- ✅ 校验和验证
- ✅ 版本兼容性检查
- ✅ 内存不足处理
- ✅ stdin/stdout 特殊处理

### P2 - 完善

- ⏸️ --skip-corrupted 模式
- ⏸️ PE配对验证
- ⏸️ 超长ID警告
- ⏸️ 磁盘空间预检

---

**文档作者**: Claude (Sonnet 4.5)
**创建日期**: 2026-01-27
**适用版本**: fq-compressor v1.0
