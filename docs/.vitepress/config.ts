import { defineConfig } from "vitepress";
import llmstxt from "vitepress-plugin-llms";
import { withMermaid } from "vitepress-plugin-mermaid";

const rawBase = process.env.VITEPRESS_BASE;
const base = rawBase
    ? rawBase.startsWith("/")
        ? rawBase.endsWith("/")
            ? rawBase
            : `${rawBase}/`
        : `/${rawBase}/`
    : "/fq-compressor/";

const englishNav = [
    { text: "Overview", link: "/en/overview/", activeMatch: "/en/overview/" },
    { text: "Whitepaper", link: "/en/whitepaper/", activeMatch: "/en/whitepaper/" },
    { text: "Architecture", link: "/en/architecture/", activeMatch: "/en/architecture/" },
    { text: "Evidence", link: "/en/benchmarks/", activeMatch: "/en/benchmarks/" },
    { text: "Academy", link: "/en/academy/", activeMatch: "/en/academy/" },
    { text: "Research", link: "/en/research/", activeMatch: "/en/research/" }
];

const chineseNav = [
    { text: "导读", link: "/zh/overview/", activeMatch: "/zh/overview/" },
    { text: "白皮书", link: "/zh/whitepaper/", activeMatch: "/zh/whitepaper/" },
    { text: "架构", link: "/zh/architecture/", activeMatch: "/zh/architecture/" },
    { text: "证据", link: "/zh/benchmarks/", activeMatch: "/zh/benchmarks/" },
    { text: "学院", link: "/zh/academy/", activeMatch: "/zh/academy/" },
    { text: "研究", link: "/zh/research/", activeMatch: "/zh/research/" }
];

const englishSidebar = {
    "/en/overview/": [
        {
            text: "Overview",
            items: [{ text: "Reader briefing", link: "/en/overview/" }]
        }
    ],
    "/en/whitepaper/": [
        {
            text: "Whitepaper",
            items: [
                { text: "Research framing", link: "/en/whitepaper/" },
                { text: "ABC pipeline", link: "/en/whitepaper/abc-pipeline" },
                { text: "SCM quality modeling", link: "/en/whitepaper/scm-quality" },
                { text: "Read reordering", link: "/en/whitepaper/reordering" },
                {
                    text: "Consensus and delta representation",
                    link: "/en/whitepaper/consensus-delta"
                }
            ]
        }
    ],
    "/en/architecture/": [
        {
            text: "Architecture",
            items: [
                { text: "System structure", link: "/en/architecture/" },
                { text: "Pipeline", link: "/en/architecture/pipeline" },
                {
                    text: "FQC format and random access",
                    link: "/en/architecture/format-random-access"
                },
                { text: "I/O and memory", link: "/en/architecture/io-memory" }
            ]
        }
    ],
    "/en/benchmarks/": [
        {
            text: "Evidence",
            items: [
                { text: "Evidence boundary", link: "/en/benchmarks/" },
                { text: "Methodology", link: "/en/benchmarks/methodology" }
            ]
        }
    ],
    "/en/academy/": [
        {
            text: "Academy",
            items: [
                { text: "Operator path", link: "/en/academy/" },
                { text: "Installation", link: "/en/academy/installation" },
                { text: "Getting started", link: "/en/academy/getting-started" },
                { text: "CLI workflows", link: "/en/academy/cli-workflows" },
                { text: "Contributing", link: "/en/academy/contributing" }
            ]
        }
    ],
    "/en/research/": [
        {
            text: "Research",
            items: [
                { text: "Research desk", link: "/en/research/" },
                { text: "References", link: "/en/research/references" },
                {
                    text: "Comparative study",
                    link: "/en/research/open-source-comparative-study"
                },
                { text: "Evolution notes", link: "/en/research/evolution-notes" }
            ]
        }
    ]
};

const chineseSidebar = {
    "/zh/overview/": [
        {
            text: "导读",
            items: [{ text: "读者导览", link: "/zh/overview/" }]
        }
    ],
    "/zh/whitepaper/": [
        {
            text: "白皮书",
            items: [
                { text: "研究框架", link: "/zh/whitepaper/" },
                { text: "ABC 流水线", link: "/zh/whitepaper/abc-pipeline" },
                { text: "SCM 质量值建模", link: "/zh/whitepaper/scm-quality" },
                { text: "Reads 重排", link: "/zh/whitepaper/reordering" },
                { text: "共识与差分表示", link: "/zh/whitepaper/consensus-delta" }
            ]
        }
    ],
    "/zh/architecture/": [
        {
            text: "架构",
            items: [
                { text: "系统结构", link: "/zh/architecture/" },
                { text: "流水线", link: "/zh/architecture/pipeline" },
                { text: "FQC 格式与随机访问", link: "/zh/architecture/format-random-access" },
                { text: "I/O 与内存", link: "/zh/architecture/io-memory" }
            ]
        }
    ],
    "/zh/benchmarks/": [
        {
            text: "证据",
            items: [
                { text: "证据边界", link: "/zh/benchmarks/" },
                { text: "方法学", link: "/zh/benchmarks/methodology" }
            ]
        }
    ],
    "/zh/academy/": [
        {
            text: "学院",
            items: [
                { text: "操作者路径", link: "/zh/academy/" },
                { text: "安装", link: "/zh/academy/installation" },
                { text: "快速开始", link: "/zh/academy/getting-started" },
                { text: "CLI 工作流", link: "/zh/academy/cli-workflows" },
                { text: "贡献", link: "/zh/academy/contributing" }
            ]
        }
    ],
    "/zh/research/": [
        {
            text: "研究",
            items: [
                { text: "研究入口", link: "/zh/research/" },
                { text: "参考文献", link: "/zh/research/references" },
                { text: "开源项目对照", link: "/zh/research/open-source-comparative-study" },
                { text: "演进思考", link: "/zh/research/evolution-notes" }
            ]
        }
    ]
};

export default withMermaid(
    defineConfig({
        title: "fq-compressor",
        description: "FASTQ compression whitepaper, architecture, and evidence portal",
        base,
        srcExclude: [
            "README.md",
            "archive/**",
            "benchmark/**",
            "dev/**",
            "superpowers/**",
            "website/**",
            "zh/ai-development-mode.md"
        ],
        locales: {
            en: {
                label: "English",
                lang: "en-US",
                link: "/en/",
                title: "fq-compressor",
                description: "FASTQ compression whitepaper, architecture, and evidence portal",
                themeConfig: {
                    nav: englishNav,
                    sidebar: englishSidebar
                }
            },
            zh: {
                label: "简体中文",
                lang: "zh-CN",
                link: "/zh/",
                title: "fq-compressor",
                description: "fq-compressor FASTQ 压缩白皮书、架构与证据站点",
                themeConfig: {
                    nav: chineseNav,
                    sidebar: chineseSidebar
                }
            }
        },
        themeConfig: {
            outline: [2, 3],
            search: {
                provider: "local"
            },
            socialLinks: [
                {
                    icon: "github",
                    link: "https://github.com/LessUp/fq-compressor"
                }
            ]
        },
        vite: {
            plugins: [llmstxt()]
        }
    })
);
