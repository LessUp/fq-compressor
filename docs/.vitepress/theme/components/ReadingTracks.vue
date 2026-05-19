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

if (import.meta.env.DEV && props.locale && !["en", "zh"].includes(props.locale)) {
    console.warn(`[ReadingTracks] Invalid locale: ${props.locale}, expected 'en' or 'zh'`);
}

const copy = {
    en: {
        kicker: "Reading tracks",
        title: "Choose a route that matches your question, then stay in that lane.",
        tracks: [
            {
                id: "01",
                audience: "Staff-level reviewer",
                title: "Evaluate the project thesis in one sitting",
                body: "Start with algorithms, then verify every public claim against the evidence contract.",
                entry: "Whitepaper -> Performance",
                outcome: "You can judge whether the public story outruns the repository.",
                href: "/en/whitepaper/"
            },
            {
                id: "02",
                audience: "Operators",
                title: "Move from install to verified archive handling",
                body: "Stay in operations when the immediate goal is to install, run, verify, or spot-check real outputs.",
                entry: "Operations -> System Design",
                outcome: "You can execute the tool without guessing hidden format rules.",
                href: "/en/academy/"
            },
            {
                id: "03",
                audience: "Contributors",
                title: "Map the code before changing anything",
                body: "Read the system blueprint with contribution guidance open beside it so architectural boundaries stay visible during edits.",
                entry: "System Design -> Operations",
                outcome: "You know which modules own parsing, compression, format, and command glue.",
                href: "/en/architecture/"
            },
            {
                id: "04",
                audience: "Research readers",
                title: "Put the design choices back into external context",
                body: "Use the reference shelf for papers, comparator repositories, and closeout-mode evolution notes.",
                entry: "References -> Algorithms",
                outcome: "You can explain which upstream ideas fq-compressor keeps, adapts, or rejects.",
                href: "/en/research/"
            }
        ],
        linkLabel: "Open this track",
        entryLabel: "Entry",
        outcomeLabel: "Outcome"
    },
    zh: {
        kicker: "阅读路线",
        title: "先按问题选择路线，再保持在同一条轨道里。",
        tracks: [
            {
                id: "01",
                audience: "高级评审 / 面试官",
                title: "在一次阅读里评估项目命题",
                body: "先读算法白皮书，再回到性能证据核对每条公开主张。",
                entry: "白皮书 -> 性能证据",
                outcome: "你可以判断公开叙事有没有超出仓库当前能证明的范围。",
                href: "/zh/whitepaper/"
            },
            {
                id: "02",
                audience: "操作者",
                title: "从安装走到一次可验证运行",
                body: "如果当前目标是安装、运行、校验或 spot-check，直接停留在操作路径。",
                entry: "操作路径 -> 系统设计",
                outcome: "你可以运行工具，同时理解格式与校验要求。",
                href: "/zh/academy/"
            },
            {
                id: "03",
                audience: "贡献者",
                title: "动手前先建立代码地图",
                body: "把系统设计与贡献流程并排阅读，先看边界，再改实现。",
                entry: "系统设计 -> 操作路径",
                outcome: "你会知道解析、压缩、格式、命令编排分别归谁负责。",
                href: "/zh/architecture/"
            },
            {
                id: "04",
                audience: "研究读者",
                title: "把设计选择放回外部语境",
                body: "参考研究部分负责论文、对照仓库，以及 closeout 阶段的演进说明。",
                entry: "参考研究 -> 白皮书",
                outcome: "你可以说明 fq-compressor 保留、改写或拒绝了哪些上游思路。",
                href: "/zh/research/"
            }
        ],
        linkLabel: "进入这条轨道",
        entryLabel: "入口",
        outcomeLabel: "结果"
    }
} as const;

const content = computed(() => copy[props.locale]);
</script>

<template>
    <section class="wp-section">
        <header class="wp-section__intro">
            <p class="wp-kicker">{{ content.kicker }}</p>
            <h2>{{ content.title }}</h2>
        </header>
        <ol class="wp-reading-tracks">
            <li v-for="track in content.tracks" :key="track.id" class="wp-reading-tracks__item">
                <div class="wp-reading-tracks__meta">
                    <span class="wp-reading-tracks__index">{{ track.id }}</span>
                    <p>{{ track.audience }}</p>
                </div>
                <div class="wp-reading-tracks__body">
                    <h3>{{ track.title }}</h3>
                    <p>{{ track.body }}</p>
                    <dl class="wp-reading-tracks__route">
                        <div>
                            <dt>{{ content.entryLabel }}</dt>
                            <dd>{{ track.entry }}</dd>
                        </div>
                        <div>
                            <dt>{{ content.outcomeLabel }}</dt>
                            <dd>{{ track.outcome }}</dd>
                        </div>
                    </dl>
                </div>
                <a class="wp-link" :href="withBase(track.href)">{{ content.linkLabel }}</a>
            </li>
        </ol>
    </section>
</template>
