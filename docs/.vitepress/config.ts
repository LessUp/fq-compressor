import { defineConfig } from "vitepress";

const englishNav = [
    { text: "Overview", link: "/en/overview/" },
    { text: "Whitepaper", link: "/en/whitepaper/" },
    { text: "Architecture", link: "/en/architecture/" },
    { text: "Benchmarks", link: "/en/benchmarks/" },
    { text: "Guides", link: "/en/guides/" },
    { text: "Resources", link: "/en/resources/" }
];

const chineseNav = [
    { text: "概览", link: "/zh/overview/" },
    { text: "白皮书", link: "/zh/whitepaper/" },
    { text: "架构", link: "/zh/architecture/" },
    { text: "基准测试", link: "/zh/benchmarks/" },
    { text: "指南", link: "/zh/guides/" },
    { text: "资源", link: "/zh/resources/" }
];

const englishSidebar = {
    "/en/overview/": [
        {
            text: "Overview",
            items: [{ text: "Purpose and scope", link: "/en/overview/" }]
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
            text: "Benchmarks",
            items: [{ text: "Evidence and methodology", link: "/en/benchmarks/" }]
        }
    ],
    "/en/guides/": [
        {
            text: "Guides",
            items: [
                { text: "Operator workflows", link: "/en/guides/" },
                { text: "Installation", link: "/en/guides/installation" },
                { text: "Getting started", link: "/en/guides/getting-started" },
                { text: "CLI usage", link: "/en/guides/cli-usage" },
                { text: "Contributing", link: "/en/guides/contributing" }
            ]
        }
    ],
    "/en/resources/": [
        {
            text: "Resources",
            items: [{ text: "References and links", link: "/en/resources/" }]
        }
    ]
};

const chineseSidebar = {
    "/zh/overview/": [
        {
            text: "概览",
            items: [{ text: "定位与范围", link: "/zh/overview/" }]
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
            text: "基准测试",
            items: [{ text: "证据与方法", link: "/zh/benchmarks/" }]
        }
    ],
    "/zh/guides/": [
        {
            text: "指南",
            items: [
                { text: "操作流程", link: "/zh/guides/" },
                { text: "安装", link: "/zh/guides/installation" },
                { text: "快速开始", link: "/zh/guides/getting-started" },
                { text: "CLI 使用", link: "/zh/guides/cli-usage" },
                { text: "贡献", link: "/zh/guides/contributing" }
            ]
        }
    ],
    "/zh/resources/": [
        {
            text: "资源",
            items: [{ text: "参考资料与链接", link: "/zh/resources/" }]
        }
    ]
};

export default defineConfig({
    title: "fq-compressor",
    description: "High-performance FASTQ compression documentation",
    base: "/fq-compressor/",
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
            description: "High-performance FASTQ compression documentation",
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
            description: "fq-compressor 高性能 FASTQ 压缩文档",
            themeConfig: {
                nav: chineseNav,
                sidebar: chineseSidebar
            }
        }
    },
    themeConfig: {
        search: {
            provider: "local"
        },
        socialLinks: [
            {
                icon: "github",
                link: "https://github.com/LessUp/fq-compressor"
            }
        ]
    }
});
