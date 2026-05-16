# Git Pages 首页组件样式修复计划

## Context

用户报告 Git Pages 首页的 TopicCardGrid 组件中，标签（tags: "叙事、数据流、方法、构建、定位、仓库"）的位置显示不正确。

## 问题分析

经过详细检查：

1. **HTML 结构正确** - 标签 `<ul class="topic-card-grid__tags">` 正确嵌套在每个卡片 `<article class="topic-card-grid__card">` 内部

2. **CSS 样式存在** - `topic-card-grid__tags` 相关样式在 `style.css` 和构建输出中都有定义

3. **可能的问题**:
   - 样式可能没有被正确应用到 Vue 组件
   - 可能存在样式优先级问题
   - 可能是 SSR hydration 问题导致样式不生效

## 修复方案

### 方案 1: 检查组件样式注入 (推荐)

检查 Vue 组件中 `<style>` 块是否使用了 `scoped`，可能导致样式不生效。

**文件**: `docs/.vitepress/theme/components/TopicCardGrid.vue`

当前组件使用 `<script setup>` 但没有 `<style>` 块，样式来自 `style.css`。这应该没问题，但需要确认：

1. 确认 `theme/index.ts` 正确导入了 `style.css`
2. 确认样式没有被 VitePress 默认样式覆盖

### 方案 2: 添加更具体的样式规则

在 `style.css` 中添加更高优先级的样式规则：

```css
/* 确保 tags 在卡片内部正确显示 */
.topic-card-grid__card {
    display: flex;
    flex-direction: column;
}

.topic-card-grid__tags {
    margin-top: auto; /* 将 tags 推到卡片底部 */
}
```

### 方案 3: 在 Vue 组件中内联样式

如果全局样式不生效，可以在 Vue 组件中添加 `<style scoped>` 块。

## 实施步骤

1. [ ] 验证当前部署的样式是否正确加载
2. [ ] 检查浏览器开发工具中是否有样式覆盖
3. [ ] 根据 Root Cause 选择修复方案
4. [ ] 本地构建测试
5. [ ] 部署验证

## 其他发现

### 需要清理的遗留文件

1. **`docs/.site/` 目录** - 3.0M 的 Next.js 旧版构建产物，应该删除
   - 这是从 Nextra 迁移到 VitePress 后遗留的旧站点

### 可能的优化

1. **GitHub Pages source 配置** - 当前设置为 `docs/` 目录，但实际通过 Actions 部署
   - 不影响功能，但可以清理配置

## 关键文件

- `docs/.vitepress/theme/style.css` - 自定义样式
- `docs/.vitepress/theme/components/TopicCardGrid.vue` - 卡片组件
- `docs/.vitepress/theme/index.ts` - 主题入口

## 验证方法

1. 本地运行 `cd docs && npm run dev`
2. 访问 http://localhost:5173/fq-compressor/en/
3. 检查首页底部的 TopicCardGrid 组件
4. 确认标签显示在每个卡片内部底部
