# fq-compressor Pages Whitepaper Overdrive Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Recast the GitHub Pages site as a bilingual systems whitepaper portal with stronger technical storytelling, sharper information architecture, and dark/light-safe diagrams that stay legible in both themes.

**Architecture:** Keep the existing VitePress bilingual foundation and default-theme extension, but replace the visual layer with a more deliberate editorial token system plus reusable Vue primitives for evidence, architecture, and references. Strengthen the major English and Chinese hub pages so each section works as a guided reading surface for advanced GitHub readers instead of a thin index.

**Tech Stack:** VitePress, Vue 3 SFCs, TypeScript, CSS custom properties, Mermaid, vitepress-plugin-llms

---

## File Map

- **Modify:** `docs/.vitepress/config.ts`
- **Modify:** `docs/.vitepress/theme/index.ts`
- **Modify:** `docs/.vitepress/theme/style.css`
- **Modify/Create:** `docs/.vitepress/theme/components/*.vue`
- **Modify:** `docs/en/index.md`
- **Modify:** `docs/en/overview/index.md`
- **Modify:** `docs/en/whitepaper/index.md`
- **Modify:** `docs/en/architecture/index.md`
- **Modify:** `docs/en/benchmarks/index.md`
- **Modify:** `docs/en/research/index.md`
- **Modify:** `docs/zh/index.md`
- **Modify:** `docs/zh/overview/index.md`
- **Modify:** `docs/zh/whitepaper/index.md`
- **Modify:** `docs/zh/architecture/index.md`
- **Modify:** `docs/zh/benchmarks/index.md`
- **Modify:** `docs/zh/research/index.md`

### Task 1: Reframe the public navigation and reader contract

**Files:**
- Modify: `docs/.vitepress/config.ts`
- Test: `cd docs && npm run build`

- [ ] **Step 1: Update navigation labels so the top bar reads like a reading program**

```ts
const englishNav = [
  { text: "Briefing", link: "/en/overview/", activeMatch: "/en/overview/" },
  { text: "Algorithms", link: "/en/whitepaper/", activeMatch: "/en/whitepaper/" },
  { text: "System Design", link: "/en/architecture/", activeMatch: "/en/architecture/" },
  { text: "Performance", link: "/en/benchmarks/", activeMatch: "/en/benchmarks/" },
  { text: "Operations", link: "/en/academy/", activeMatch: "/en/academy/" },
  { text: "References", link: "/en/research/", activeMatch: "/en/research/" }
];
```

- [ ] **Step 2: Mirror the same intent in Chinese**

```ts
const chineseNav = [
  { text: "导读", link: "/zh/overview/", activeMatch: "/zh/overview/" },
  { text: "算法白皮书", link: "/zh/whitepaper/", activeMatch: "/zh/whitepaper/" },
  { text: "系统设计", link: "/zh/architecture/", activeMatch: "/zh/architecture/" },
  { text: "性能证据", link: "/zh/benchmarks/", activeMatch: "/zh/benchmarks/" },
  { text: "操作路径", link: "/zh/academy/", activeMatch: "/zh/academy/" },
  { text: "参考研究", link: "/zh/research/", activeMatch: "/zh/research/" }
];
```

- [ ] **Step 3: Keep build coverage after the IA relabel**

Run: `cd docs && npm run build`
Expected: `build complete`

### Task 2: Replace the phase-one portal styling with a research-grade theme layer

**Files:**
- Modify: `docs/.vitepress/theme/style.css`
- Modify: `docs/.vitepress/theme/index.ts`
- Modify/Create: `docs/.vitepress/theme/components/*.vue`
- Test: `cd docs && npm run build`

- [ ] **Step 1: Replace the current portal tokens with stronger paper, ink, grid, and signal roles**

```css
:root {
  --portal-ink-1: oklch(...);
  --portal-paper: oklch(...);
  --portal-grid: color-mix(in oklab, var(--vp-c-brand-1) 14%, transparent);
  --portal-diagram-line: color-mix(in oklab, var(--vp-c-brand-1) 48%, var(--vp-c-text-1));
}
```

- [ ] **Step 2: Add reusable visual primitives that render as HTML/CSS instead of brittle theme-specific SVG**

```vue
<section class="signal-strip">
  <article class="signal-strip__item">
    <p class="signal-strip__value">3.97x</p>
    <h3>Compression density</h3>
    <p>Positioned beside decode speed, archive semantics, and evidence provenance.</p>
  </article>
</section>
```

- [ ] **Step 3: Harden Mermaid and inline SVG rendering so both themes stay readable**

```css
.mermaid svg [fill]:not([fill="none"]) {
  fill: var(--portal-diagram-fill);
}

.mermaid svg [stroke]:not([stroke="none"]) {
  stroke: var(--portal-diagram-line);
}
```

- [ ] **Step 4: Build after theme work**

Run: `cd docs && npm run build`
Expected: `build complete`

### Task 3: Rewrite the English and Chinese landing surfaces around the new thesis

**Files:**
- Modify: `docs/en/index.md`
- Modify: `docs/en/overview/index.md`
- Modify: `docs/en/whitepaper/index.md`
- Modify: `docs/en/architecture/index.md`
- Modify: `docs/en/benchmarks/index.md`
- Modify: `docs/en/research/index.md`
- Modify: `docs/zh/index.md`
- Modify: `docs/zh/overview/index.md`
- Modify: `docs/zh/whitepaper/index.md`
- Modify: `docs/zh/architecture/index.md`
- Modify: `docs/zh/benchmarks/index.md`
- Modify: `docs/zh/research/index.md`
- Test: `cd docs && npm run build`

- [ ] **Step 1: Rewrite the homepages so the first read feels like a whitepaper abstract with linked evidence**

```md
<EvidenceGrid locale="en" />
<ArchitectureAtlas locale="en" />
<ReadingTracks locale="en" />
<CitationCluster locale="en" />
```

- [ ] **Step 2: Upgrade the section hubs from short lists to guided reading pages with thesis, scope, and next-step links**

```md
## What this section proves

- Which claim this section supports
- Which source files and benchmark artifacts anchor it
- Where the reader should go next
```

- [ ] **Step 3: Mirror the same narrative structure for Chinese readers**

```md
## 这一节负责证明什么

- 它支撑哪一条公开主张
- 对应哪些源码与 benchmark 产物
- 读完之后应该继续看哪里
```

- [ ] **Step 4: Build after content rewrite**

Run: `cd docs && npm run build`
Expected: `build complete`

### Task 4: Validate the redesign against repo standards and ship it

**Files:**
- Modify: all changed docs files above
- Test: `./scripts/lint.sh format-check`
- Test: `./scripts/test.sh clang-debug`
- Test: `cd docs && npm run build`

- [ ] **Step 1: Run repository format and test checks**

Run: `./scripts/lint.sh format-check && ./scripts/test.sh clang-debug`
Expected: `All tests passed!`

- [ ] **Step 2: Rebuild the docs site**

Run: `cd docs && npm run build`
Expected: `build complete`

- [ ] **Step 3: Review changed files before commit**

Run: `git --no-pager diff -- docs/.vitepress docs/en docs/zh docs/superpowers/plans/2026-05-18-pages-whitepaper-overdrive.md`
Expected: a coherent docs-only redesign diff

- [ ] **Step 4: Commit with a docs-focused message**

```bash
git add docs/.vitepress docs/en docs/zh docs/superpowers/plans/2026-05-18-pages-whitepaper-overdrive.md
git commit -m "docs: overdrive GitHub Pages whitepaper portal"
```
