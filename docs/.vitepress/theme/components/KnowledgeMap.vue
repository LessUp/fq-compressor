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
        kicker: "Knowledge map",
        title: "Read the documentation as a sequence of technical questions",
        summary:
            "Each section exists to answer a specific question about the system. Start where your uncertainty is highest and follow the evidence trail from there.",
        nodes: [
            {
                title: "Overview",
                href: "/en/overview/",
                question: "What problem is this project trying to solve?",
                answer: "Project scope, audience, and the durable claims worth carrying into deeper reading."
            },
            {
                title: "Whitepaper",
                href: "/en/whitepaper/",
                question: "What thesis is being argued?",
                answer: "Why FASTQ compression should be judged as a system design problem, not a single benchmark line."
            },
            {
                title: "Architecture",
                href: "/en/architecture/",
                question: "How is that thesis implemented?",
                answer: "Pipeline boundaries, format responsibilities, and where random access lives in the codebase."
            },
            {
                title: "Benchmarks",
                href: "/en/benchmarks/",
                question: "Which numbers are actually supported?",
                answer: "Method, dataset provenance, and links to the benchmark artifacts behind public claims."
            },
            {
                title: "Guides",
                href: "/en/guides/",
                question: "How do I operate or verify it?",
                answer: "Build, test, run, and contributor-facing workflows grounded in repository commands."
            },
            {
                title: "Resources",
                href: "/en/resources/",
                question: "Where is the source of truth?",
                answer: "Repository, specs, releases, and archive boundaries for follow-up research."
            }
        ]
    },
    zh: {
        kicker: "知识地图",
        title: "把文档当作一串技术问题来阅读",
        summary:
            "每个栏目都应该回答一个明确的问题。先从你最不确定的地方开始，再顺着证据链继续深入。",
        nodes: [
            {
                title: "概览",
                href: "/zh/overview/",
                question: "这个项目到底在解决什么问题？",
                answer: "说明项目范围、读者对象，以及值得继续追踪的稳定主张。"
            },
            {
                title: "白皮书",
                href: "/zh/whitepaper/",
                question: "它想论证的核心命题是什么？",
                answer: "为什么 FASTQ 压缩更像一个系统设计问题，而不只是排行榜上的一行数字。"
            },
            {
                title: "架构",
                href: "/zh/architecture/",
                question: "这个命题在实现上如何落地？",
                answer: "展示流水线边界、格式职责，以及随机访问在代码中的位置。"
            },
            {
                title: "基准测试",
                href: "/zh/benchmarks/",
                question: "哪些数字真的有证据支持？",
                answer: "梳理方法、数据集来源，以及公开主张背后的结果文件。"
            },
            {
                title: "指南",
                href: "/zh/guides/",
                question: "我该如何运行、验证或维护它？",
                answer: "对应仓库中的构建、测试、命令使用与贡献流程。"
            },
            {
                title: "资源",
                href: "/zh/resources/",
                question: "后续追踪该看哪里？",
                answer: "集中列出仓库、规范、发布与归档边界，便于继续研究。"
            }
        ]
    }
} as const;

const content = computed(() => copy[props.locale]);
</script>

<template>
    <div class="portal-shell">
        <section class="portal-section knowledge-map" :aria-label="content.title">
            <div class="portal-section__header">
                <div>
                    <p class="portal-kicker">{{ content.kicker }}</p>
                    <h2>{{ content.title }}</h2>
                </div>
                <p class="knowledge-map__summary">{{ content.summary }}</p>
            </div>
            <ol class="knowledge-map__grid">
                <li v-for="(node, index) in content.nodes" :key="node.href" class="knowledge-map__item">
                    <span class="knowledge-map__index">{{ String(index + 1).padStart(2, "0") }}</span>
                    <h3>
                        <a :href="withBase(node.href)">{{ node.title }}</a>
                    </h3>
                    <p class="knowledge-map__question">{{ node.question }}</p>
                    <p class="knowledge-map__answer">{{ node.answer }}</p>
                </li>
            </ol>
        </section>
    </div>
</template>
