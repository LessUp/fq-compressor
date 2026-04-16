import { useRouter } from 'next/router'

const config = {
  logo: (
    <>
      <span className="font-bold text-xl">fq-compressor</span>
      <span className="ml-2 text-sm text-gray-500 dark:text-gray-400 hidden sm:inline">
        High-performance FASTQ compression
      </span>
    </>
  ),
  project: {
    link: 'https://github.com/LessUp/fq-compressor',
  },
  docsRepositoryBase: 'https://github.com/LessUp/fq-compressor/tree/master/docs/website',
  useNextSeoProps() {
    const { asPath } = useRouter()
    if (asPath !== '/') {
      return {
        titleTemplate: '%s – fq-compressor',
      }
    }
  },
  head: (
    <>
      <meta name="viewport" content="width=device-width, initial-scale=1.0" />
      <meta name="description" content="High-performance FASTQ compression tool with Assembly-based Compression (ABC) and random access" />
      <meta name="og:title" content="fq-compressor" />
      <link rel="icon" type="image/svg+xml" href="/favicon.svg" />
    </>
  ),
  search: {
    placeholder: 'Search documentation...',
  },
  toc: {
    title: 'On This Page',
  },
  editLink: {
    text: 'Edit this page on GitHub →',
  },
  feedback: {
    content: 'Question? Give us feedback →',
    labels: 'documentation',
  },
  footer: {
    text: (
      <span>
        MIT {new Date().getFullYear()} ©{' '}
        <a href="https://github.com/LessUp" target="_blank" rel="noreferrer">
          LessUp
        </a>
        . Built with Nextra & Next.js.
      </span>
    ),
  },
  i18n: [
    { locale: 'en', text: 'English' },
    { locale: 'zh', text: '简体中文' },
  ],
  darkMode: true,
  nextThemes: {
    defaultTheme: 'system',
    storageKey: 'theme',
  },
  banner: {
    key: 'v0.1.0-release',
    text: (
      <a href="https://github.com/LessUp/fq-compressor/releases" target="_blank" rel="noreferrer">
        🎉 fq-compressor v0.1.0 is released. Read more →
      </a>
    ),
  },
  chat: {
    link: 'https://github.com/LessUp/fq-compressor/discussions',
    icon: (
      <svg width="24" height="24" fill="currentColor" viewBox="0 0 24 24">
        <path d="M12 2C6.48 2 2 6.03 2 11c0 2.76 1.36 5.22 3.5 6.8V22l4.09-2.24c.93.24 1.92.37 2.91.37 5.52 0 10-4.03 10-9s-4.48-9-10-9zm0 16c-.83 0-1.63-.1-2.4-.29L7.5 18.5v-2.07C6.14 15.64 5 13.46 5 11c0-3.87 3.58-7 8-7s8 3.13 8 7-3.58 7-8 7z"/>
      </svg>
    ),
  },
  sidebar: {
    titleComponent({ title, type }) {
      if (type === 'separator') {
        return <span className="cursor-default">{title}</span>
      }
      return <>{title}</>
    },
    defaultMenuCollapseLevel: 1,
    toggleButton: true,
  },
  components: {
    pre: ({ children }) => (
      <pre className="nextra-pre">{children}</pre>
    ),
  },
}

export default config
