# fq-compressor 规格更新：CLI 细化、版本化与 I/O 取舍（2026-01-14）

## 背景

- 用户对 `.bz2/.xz` 输入与 `.fqc.gz` 外层封装输出的价值提出质疑。
- 项目采用容器开发（GCC 15 / Clang 21 / CMake 4.x / Conan 2.x），工具链版本显著高于最小要求，需要在规格中明确“推荐环境”。
- 为避免未来格式升级（校验算法、codec、header 扩展）引发兼容性问题，需要在容器格式中预先设计前向兼容机制。

## 变更

- `.kiro/specs/fq-compressor/requirements.md`
  - `Requirement 1.1.1` I/O 支持调整：
    - `.bz2/.xz` 输入降级为可选扩展（非 MVP，CPU 成本较高）。
    - 移除 `.fqc.gz` 外层封装输出需求，并明确其会破坏随机访问语义；如需分发建议在归档外部自行二次压缩。
  - `Requirement 6` CLI 设计细化：
    - 明确子命令 `compress/decompress/info/verify`，补齐全局选项、I/O 约定、默认值、互斥关系。
    - 增加退出码约定（参数错误/I/O 错误/格式不兼容/校验失败/不支持 codec）。
  - `Requirement 7` 增加容器开发推荐工具链版本说明。

- `.kiro/specs/fq-compressor/design.md`
  - 容器格式前向兼容：
    - Global Header 增加 `GlobalHeaderSize`，用于跳过未知扩展字段。
    - BlockHeader 增加 `header_size` 与 `checksum_type`，用于 header 与校验算法升级。
    - `codec_*` 字段增加 `(family,version)` 的版本化约定，便于算法升级与兼容判断。
  - 依赖与示例对齐：补充 `zstd` 作为通用子流压缩后端候选。

- `.kiro/specs/fq-compressor/tasks.md`
  - 将 `.fqc.gz` 内置封装输出实现任务改为“文档建议”（外部二次压缩），避免破坏随机访问。
  - ID 子流通用压缩器表述更新为 `LZMA/Zstd` 以匹配依赖与可选后端。

## 影响

- I/O 能力更聚焦核心价值：优先保证 `.fqc` 的随机访问与可验证性，不在容器内叠加外层压缩。
- CLI 规格可直接落地实现，并为后续扩展（子流选择性解码、更多 verify 模式、更多 codec）预留空间。
- 格式升级路径更清晰：新增字段/新算法可在不破坏旧 reader 的前提下演进。

## 回退方案

- 回滚上述三份 spec 文件以及本 changelog 文件即可。
