<script setup lang="ts">
import { withBase } from "vitepress";
import { computed } from "vue";

const props = withDefaults(
    defineProps<{
        locale?: "en" | "zh";
    }>(),
    {
        locale: "en"
    }
);

const copy = {
    en: {
        kicker: "Topic grid",
        title: "Start with the slice of the system you need to trust",
        tagsLabel: "topic tags",
        summary:
            "These entry points are intentionally narrow. They help a reader inspect one engineering concern without being forced through the entire narrative first.",
        cards: [
            {
                title: "Claim surface",
                href: "/en/whitepaper/",
                body: "Read the public argument, the intended proof points, and the scope limits around current claims.",
                tags: ["narrative", "scope", "trade-offs"]
            },
            {
                title: "Pipeline and format",
                href: "/en/architecture/",
                body: "Inspect how parsing, analysis, compression, storage, and indexed retrieval are separated in the system.",
                tags: ["data flow", "boundaries", "FQC"]
            },
            {
                title: "Evidence trail",
                href: "/en/benchmarks/",
                body: "Tie every headline metric back to datasets, methodology, and tracked benchmark artifacts.",
                tags: ["method", "datasets", "results"]
            },
            {
                title: "Operator workflows",
                href: "/en/guides/",
                body: "Jump straight to build, test, command execution, and maintenance procedures rooted in repository scripts.",
                tags: ["build", "verify", "operate"]
            },
            {
                title: "Project framing",
                href: "/en/overview/",
                body: "Use the orientation layer when you need the shortest explanation of audience, scope, and reading order.",
                tags: ["orientation", "audience", "map"]
            },
            {
                title: "Source references",
                href: "/en/resources/",
                body: "Follow links to source code, specifications, releases, and archival material with clear trust boundaries.",
                tags: ["repo", "specs", "archive"]
            }
        ]
    },
    zh: {
        kicker: "主题网格",
        title: "从你最需要建立信任的那一层系统开始",
        tagsLabel: "主题标签",
        summary:
            "这些入口刻意保持聚焦，让读者可以先检查单个工程问题，而不是被迫按整条叙事顺序阅读。",
        cards: [
            {
                title: "主张边界",
                href: "/zh/whitepaper/",
                body: "先看公开叙事、希望证明的点，以及当前主张主动保留的范围限制。",
                tags: ["叙事", "范围", "取舍"]
            },
            {
                title: "流水线与格式",
                href: "/zh/architecture/",
                body: "检查解析、分析、压缩、存储与索引检索如何在系统中解耦。",
                tags: ["数据流", "边界", "FQC"]
            },
            {
                title: "证据链",
                href: "/zh/benchmarks/",
                body: "把所有 headline 指标重新连回数据集、方法与已跟踪的结果文件。",
                tags: ["方法", "数据集", "结果"]
            },
            {
                title: "操作流程",
                href: "/zh/guides/",
                body: "直接进入构建、测试、命令执行与维护流程，并以仓库脚本为准。",
                tags: ["构建", "验证", "运行"]
            },
            {
                title: "项目定位",
                href: "/zh/overview/",
                body: "当你需要最短路径了解对象、范围与阅读顺序时，从这里开始。",
                tags: ["定位", "读者", "地图"]
            },
            {
                title: "参考来源",
                href: "/zh/resources/",
                body: "继续追踪源码、规范、发布与归档材料，并明确各自的可信边界。",
                tags: ["仓库", "规范", "归档"]
            }
        ]
    }
} as const;

const content = computed(() => copy[props.locale]);
</script>

<template>
    <div class="portal-shell">
        <section class="portal-section topic-card-grid" :aria-label="content.title">
            <div class="portal-section__header">
                <div>
                    <p class="portal-kicker">{{ content.kicker }}</p>
                    <h2>{{ content.title }}</h2>
                </div>
                <p class="topic-card-grid__summary">{{ content.summary }}</p>
            </div>
            <div class="topic-card-grid__grid">
                <article v-for="card in content.cards" :key="card.href" class="topic-card-grid__card">
                    <h3>
                        <a :href="withBase(card.href)">{{ card.title }}</a>
                    </h3>
                    <p>{{ card.body }}</p>
                    <ul class="topic-card-grid__tags" :aria-label="content.tagsLabel">
                        <li v-for="tag in card.tags" :key="tag">{{ tag }}</li>
                    </ul>
                </article>
            </div>
        </section>
    </div>
</template>
