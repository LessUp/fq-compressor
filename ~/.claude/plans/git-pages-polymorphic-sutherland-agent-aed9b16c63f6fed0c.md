# Git Pages 问题分析与修复方案

## 发现的关键问题

### 🔴 严重问题

1. **GitHub Pages 未启用**
   - API 返回: `"has_pages": false`
   - Pages 端点返回 404
   - 网站访问返回 404: https://lessup.github.io/ai-inference-hpc/

2. **工作流分支名不匹配**
   - 仓库默认分支: `master`
   - 工作流触发条件: `branches: [main]`
   - 结果: 工作流从未被触发

3. **工作流仓库名不匹配**
   - 工作流部署条件: `github.repository == 'LessUp/ai-inference-hpc'`
   - 实际仓库名: `AICL-Lab/ai-inference-hpc`
   - 结果: 即使工作流运行，部署也会被跳过

4. **VitePress base path 配置**
   - 当前配置: `base: '/ai-inference-hpc/'`
   - 实际 URL 应为: `https://aicl-lab.github.io/ai-inference-hpc/`
   - 这个配置是正确的

### 🟡 潜在问题

5. **Vue 组件未使用 withBase()**
   - `FeatureCard.vue` 第17行: `<a v-if="link" :href="link">` - 直接使用 link
   - `FeaturedProjects.vue` 第17行: `:href="project.links.docs"` - 未处理 base path
   - 如果 link 是相对路径，在 GitHub Pages 上会失效

6. **工作流缓存配置不完整**
   - 只缓存 `docs/node_modules`
   - 未缓存 VitePress 缓存目录

### 🟢 正常部分

- VitePress 配置正确
- 双语文档完整
- 构建输出存在 (8.1M)
- 无 `.site` 遗留目录 (已清理或不存在于此仓库)

---

## 修复方案

### 步骤 1: 启用 GitHub Pages

在 GitHub 仓库设置中:
1. 进入 Settings > Pages
2. Source 选择: "GitHub Actions"
3. 或者使用 gh CLI:
   ```bash
   gh api -X POST repos/AICL-Lab/ai-inference-hpc/pages \
     -f source='{"branch":"master","path":"/docs"}'
   ```

### 步骤 2: 修复工作流文件 `.github/workflows/docs.yml`

```yaml
# 修改触发分支
on:
  push:
    branches: [master]  # 改为 master
    paths:
      - 'docs/**'
      - '.github/workflows/docs.yml'

# 修改部署条件
deploy:
  needs: build
  if: github.event_name == 'push' && github.repository == 'AICL-Lab/ai-inference-hpc'  # 修改仓库名
```

### 步骤 3: 修复 Vue 组件中的链接问题

修改 `docs/.vitepress/components/FeatureCard.vue`:
```vue
<script setup lang="ts">
import { withBase } from 'vitepress'

const props = defineProps<{
  title: string
  description?: string
  icon?: string
  link?: string
}>()

const resolvedLink = computed(() => props.link ? withBase(props.link) : '')
</script>

<template>
  <!-- ... -->
  <a v-if="link" :href="resolvedLink" class="feature-link">
    Learn more →
  </a>
  <!-- ... -->
</template>
```

修改 `docs/.vitepress/components/FeaturedProjects.vue`:
```vue
<script setup lang="ts">
import { withBase } from 'vitepress'
import type { Project } from '../data/projects.data'

const getDocsUrl = (project: Project) => withBase(project.links.docs)
</script>

<template>
  <!-- ... -->
  <a :href="getDocsUrl(project)" class="featured-card">
  <!-- ... -->
  </a>
</template>
```

### 步骤 4: 更新 .gitignore

```gitignore
# 在现有 .gitignore 中添加:
docs/.vitepress/cache/
docs/.vitepress/dist/

# 如果有 node_modules 在 docs 目录
docs/node_modules/
```

### 步骤 5: 添加工作流缓存优化

```yaml
- name: Cache VitePress cache
  uses: actions/cache@v4
  with:
    path: docs/.vitepress/cache
    key: ${{ runner.os }}-vitepress-cache-${{ hashFiles('docs/**/*.md') }}
    restore-keys: |
      ${{ runner.os }}-vitepress-cache-
```

---

## 执行顺序

1. **立即修复**: 工作流分支和仓库名 → 推送到 master
2. **启用 Pages**: 通过 GitHub UI 或 API
3. **修复组件**: 添加 withBase() 处理
4. **优化缓存**: 可选，稍后进行

---

## 验证步骤

1. 检查 Pages 是否启用:
   ```bash
   gh api repos/AICL-Lab/ai-inference-hpc/pages
   ```

2. 检查工作流运行:
   ```bash
   gh api repos/AICL-Lab/ai-inference-hpc/actions/runs
   ```

3. 访问网站:
   ```bash
   curl -I https://aicl-lab.github.io/ai-inference-hpc/
   ```

---

## 预期结果

- 网站可访问: https://aicl-lab.github.io/ai-inference-hpc/
- 返回 200 状态码
- 所有内部链接正常工作
- 双语切换正常

