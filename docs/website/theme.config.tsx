import { useRouter } from 'next/router'

const config = {
  logo: (
    <>
      <span className="font-bold text-xl flex items-center gap-2">
        <svg 
          width="24" 
          height="24" 
          viewBox="0 0 24 24" 
          fill="none" 
          xmlns="http://www.w3.org/2000/svg"
          className="text-primary-600 dark:text-primary-400"
        >
          <path 
            d="M12 2C6.48 2 2 6.48 2 12C2 17.52 6.48 22 12 22C17.52 22 22 17.52 22 12C22 6.48 17.52 2 12 2ZM12 20C7.59 20 4 16.41 4 12C4 7.59 7.59 4 12 4C16.41 4 20 7.59 20 12C20 16.41 16.41 20 12 20Z" 
            fill="currentColor"
          />
          <path 
            d="M8 12C8 9.79 9.79 8 12 8C14.21 8 16 9.79 16 12" 
            stroke="currentColor" 
            strokeWidth="2" 
            strokeLinecap="round"
          />
          <circle cx="12" cy="12" r="2" fill="currentColor"/>
        </svg>
        fq-compressor
      </span>
    </>
  ),
  project: {
    link: 'https://github.com/LessUp/fq-compressor',
  },
  docsRepositoryBase: 'https://github.com/LessUp/fq-compressor/tree/master/docs/website',
  useNextSeoProps() {
    const { asPath } = useRouter()
    const title = asPath === '/' 
      ? 'fq-compressor - High-performance FASTQ Compression' 
      : '%s – fq-compressor'
    return {
      titleTemplate: title,
      description: 'High-performance FASTQ compression tool with Assembly-based Compression (ABC) and O(1) random access',
      openGraph: {
        type: 'website',
        locale: 'en_US',
        url: 'https://lessup.github.io/fq-compressor',
        siteName: 'fq-compressor',
      },
      twitter: {
        cardType: 'summary_large_image',
      },
    }
  },
  head: (
    <>
      <meta name="viewport" content="width=device-width, initial-scale=1.0" />
      <meta name="description" content="High-performance FASTQ compression tool with Assembly-based Compression (ABC) and random access" />
      <meta property="og:title" content="fq-compressor" />
      <meta property="og:description" content="High-performance FASTQ compression with 3.97x ratio and O(1) random access" />
      <link rel="icon" type="image/svg+xml" href="/fq-compressor/favicon.svg" />
      <link rel="preconnect" href="https://fonts.googleapis.com" />
      <link rel="preconnect" href="https://fonts.gstatic.com" crossOrigin="anonymous" />
      <link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700&family=JetBrains+Mono:wght@400;500&display=swap" rel="stylesheet" />
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
      <div className="flex flex-col gap-2">
        <span>
          MIT {new Date().getFullYear()} ©{' '}
          <a href="https://github.com/LessUp" target="_blank" rel="noreferrer" className="hover:text-primary-600 transition-colors">
            LessUp
          </a>
        </span>
        <span className="text-xs text-gray-500 dark:text-gray-400">
          Built with Nextra, Next.js & Tailwind CSS
        </span>
      </div>
    ),
  },
  // File-based i18n - we handle this through file structure
  // Nextra will use the path-based navigation
  darkMode: true,
  nextThemes: {
    defaultTheme: 'system',
    storageKey: 'fqc-theme',
  },
  banner: {
    key: 'v0.2.0-release',
    text: (
      <a href="https://github.com/LessUp/fq-compressor/releases" target="_blank" rel="noreferrer">
        🎉 fq-compressor v0.2.0 is released. Read more →
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
    defaultMenuCollapseLevel: 1,
    toggleButton: true,
  },
  navigation: {
    prev: true,
    next: true,
  },
}

export default config
