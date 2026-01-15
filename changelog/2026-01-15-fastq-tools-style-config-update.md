# fastq-tools 代码规范配置更新

- 调整 .clang-format 的 include 分组顺序，改为标准库/第三方/项目内的顺序。
- clang-tidy 禁用 modernize-use-trailing-return-type，避免强制尾随返回类型。
- 扩展 .editorconfig 全局默认缩进与字符集规则，补充 Makefile 规则。
- 完善 .gitmessage.txt 的 subject 约束描述。
