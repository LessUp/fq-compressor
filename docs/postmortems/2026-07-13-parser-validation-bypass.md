# `FastqParser::validateSequence` 忽略 `validBases`

- 日期：2026-07-13
- 严重度：high
- 状态：closed
- 引入点：v2 phase 2（pre-75b7400）
- 相关：`src/io/fastq_parser.cpp`

## 症状

扩展 `ParserOptions::validBases` 以包含完整 IUPAC 字母表后，解析器接受的符号集合并未发生变化。即便 profile 已允许，非 A/C/G/T/N 符号仍被拒绝。

## 复现

配置一个解析器，使其 `validBases` 包含 IUPAC 歧义码（R、Y、S、W、K、M、B、D、H、V，含大小写），然后输入一条包含其中某个符号的记录。

## 调查

- 最初怀疑该选项未在解析器构造路径中正确传递；追踪后确认它确实到达了 `validateSequence`。
- 发现 `validateSequence` 忽略了该选项，转而调用一个硬编码的辅助函数，而该辅助函数只认识 A/C/G/T/N。

## 根因

`validateSequence` 绕过了已配置的字母表，委托给一个硬编码的校验器，因此 `ParserOptions::validBases` 在序列校验中属于无效配置（dead configuration）。

## 修复

将校验流程改为经由已配置的字母表进行，并同步更新硬编码辅助函数，使两条路径都遵循 `validBases`。v2 现在针对已配置的完整大小写 IUPAC 核苷酸字母表进行校验，并无损保留异常符号。

## 验证

`fastq_parser_test` 与 `v2_archive_engine_test` 在混合符号的单端与双端输入下均通过；异常碱基经完整的压缩/解压往返后仍然存活。

## 后续与教训

解析器测试矩阵现已纳入 IUPAC 符号，使此类回归在本地即可被捕获。
