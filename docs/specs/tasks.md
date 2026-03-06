# fq-compressor 实施计划概览

## 分阶段规划

项目实施可以分为 5 个阶段：

### Phase 1: Skeleton & Format

重点内容：

- 项目骨架搭建
- CMake / Conan / CI 基础设施
- `.fqc` 格式基础读写
- CLI 基础框架
- 基础测试体系

### Phase 2: Spring 核心算法集成

重点内容：

- 分析参考实现
- 提炼可复用算法逻辑
- 全局分析与重排
- Reorder Map
- Block 压缩核心
- Quality / ID 压缩逻辑

### Phase 3: TBB 并行流水线

重点内容：

- Reader / Compressor / Writer 流水线拆分
- 并行调度
- 吞吐与内存控制
- 完整压缩 / 解压引擎

### Phase 4: 优化与扩展

重点内容：

- 异步 IO
- 长读支持
- Paired-End 支持
- 更完善的输入场景覆盖

### Phase 5: 验证与发布

重点内容：

- 完整性验证
- 集成测试
- Benchmark
- 文档完善
- 自动化发布

## 当前可见完成情况

结合仓库现状，以下内容已经基本具备：

- 构建系统
- 测试目录
- benchmark 框架
- README 与多份设计文档
- CI 初版
- 主要命令入口

## 当前最需要继续做的事情

- 统一文档体系
- 清理目录职责
- 修复和统一 GitHub Actions
- 建立 GitBook / Pages 发布链路
- 建立 tag 驱动的 GitHub Release 自动发布

## 目录治理与实施关系

### 应保留

- `src/`
- `include/`
- `tests/`
- `docs/`
- `.github/workflows/`
- `scripts/`

### 应逐步收敛或移除

- `Testing/`
- `ref-projects/`
- `.spec-workflow/`
- `.kiro/specs/fq-compressor/`

## 建议的执行顺序

1. 统一 README 与 docs 入口
2. 将规格文档融合到 `docs/specs/`
3. 建立双语 GitBook
4. 修 CI / Release / Pages
5. 最后再删除旧目录

## 注意事项

- 目录删除动作应放在确认文档迁移完成之后
- `tests/` 与 `Testing/` 的职责必须明确区分
- `ref-projects/` 的价值应以文档形式保留，而非继续以目录形式存在
