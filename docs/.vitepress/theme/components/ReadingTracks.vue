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
        title: "Choose a reading track, not a linear tour.",
        tracks: [
            {
                id: "01",
                audience: "Interview loop",
                title: "Project claim in one sitting",
                body: "Start with the whitepaper, then verify every headline in the evidence section.",
                href: "/en/whitepaper/"
            },
            {
                id: "02",
                audience: "Operators",
                title: "From install to verified archive",
                body: "Use the academy lane when your job is to install, run, verify, or spot-check outputs.",
                href: "/en/academy/"
            },
            {
                id: "03",
                audience: "Contributors",
                title: "Map the code before touching it",
                body: "Pair architecture with the academy contribution flow to understand boundaries before edits.",
                href: "/en/architecture/"
            },
            {
                id: "04",
                audience: "Research readers",
                title: "Context around the design choices",
                body: "Use the research section for papers, comparators, and closeout-mode evolution notes.",
                href: "/en/research/"
            }
        ],
        linkLabel: "Open this track"
    },
    zh: {
        title: "不要线性通读，先选一条阅读轨道。",
        tracks: [
            {
                id: "01",
                audience: "面试官 / 评审",
                title: "一次看完项目主张",
                body: "先读白皮书，再到证据部分核对所有 headline。",
                href: "/zh/whitepaper/"
            },
            {
                id: "02",
                audience: "操作者",
                title: "从安装到一次可验证运行",
                body: "当目标是安装、运行、校验或 spot-check 时，直接走学院路径。",
                href: "/zh/academy/"
            },
            {
                id: "03",
                audience: "贡献者",
                title: "动手前先建立代码地图",
                body: "把架构与学院中的贡献流程一起看，先理解边界，再改代码。",
                href: "/zh/architecture/"
            },
            {
                id: "04",
                audience: "研究读者",
                title: "把设计选择放回外部语境",
                body: "研究部分负责论文、对照项目与 closeout 阶段的演进说明。",
                href: "/zh/research/"
            }
        ],
        linkLabel: "进入这条轨道"
    }
} as const;

const content = computed(() => copy[props.locale]);
</script>

<template>
    <section class="wp-section">
        <header class="wp-section__intro">
            <h2>{{ content.title }}</h2>
        </header>
        <ol class="reading-tracks">
            <li v-for="track in content.tracks" :key="track.id" class="reading-tracks__item">
                <div class="reading-tracks__meta">
                    <span class="reading-tracks__index">{{ track.id }}</span>
                    <p>{{ track.audience }}</p>
                </div>
                <div class="reading-tracks__body">
                    <h3>{{ track.title }}</h3>
                    <p>{{ track.body }}</p>
                </div>
                <a class="wp-link" :href="withBase(track.href)">{{ content.linkLabel }}</a>
            </li>
        </ol>
    </section>
</template>
