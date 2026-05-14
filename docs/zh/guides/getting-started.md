# 快速开始

当你已经拿到 `fqc` 并想最快完成一次 `.fqc` 往返校验时，读这一页即可。
如果你还没有二进制，请先看 [安装](./installation)；如果你需要命令细节，继续看 [CLI 使用](./cli-usage)；
如果你需要源码和基准出处，结合 [资源](/zh/resources/) 一起看。

## 1. 检查二进制

```bash
build/clang-debug/src/fqc --version
build/clang-debug/src/fqc --help
```

## 2. 做一次烟雾级往返验证

```bash
cat > sample.fastq <<'FASTQ'
@read_1
ACGTACGT
+
IIIIIIII
@read_2
TGCATGCA
+
JJJJJJJJ
FASTQ

build/clang-debug/src/fqc compress -i sample.fastq -o sample.fqc
build/clang-debug/src/fqc verify -i sample.fqc
build/clang-debug/src/fqc decompress -i sample.fqc -o restored.fastq
cmp sample.fastq restored.fastq
```

如果 `cmp` 没有输出，就说明恢复后的 FASTQ 与输入一致。

## 3. 运行仓库常规检查

```bash
./scripts/test.sh clang-debug
./scripts/lint.sh format-check
```

## 继续阅读

- [CLI 使用](./cli-usage)：查看命令模式与关键选项
- [安装](./installation)：查看 release 与源码构建入口
- [贡献](./contributing)：查看 closeout 模式下的仓库流程与质量门槛
- [论文与项目](/zh/resources/papers-and-projects)：查看相关工作
