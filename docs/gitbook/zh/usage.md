# 使用说明

## 构建

```bash
conan install . --build=missing -of=build/clang-release -s build_type=Release -s compiler.cppstd=20
cmake --preset clang-release
cmake --build --preset clang-release -j$(nproc)
```

## 测试

```bash
./scripts/test.sh clang-debug
```

## 常见命令

### 压缩

```bash
fqc compress -i input.fastq -o output.fqc
```

### 解压

```bash
fqc decompress -i output.fqc -o restored.fastq
```

### 查看归档信息

```bash
fqc info output.fqc
```

### 校验归档

```bash
fqc verify output.fqc
```

## 文档说明

更完整的中文文档和阶段性设计记录，请返回 `docs/` 主目录查看。
