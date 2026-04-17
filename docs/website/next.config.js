const withNextra = require('nextra').default({
  theme: 'nextra-theme-docs',
  themeConfig: './theme.config.tsx',
  defaultShowCopyCode: true,
  staticImage: true,
})

/** @type {import('next').NextConfig} */
const nextConfig = {
  output: 'export',
  distDir: '../.site',
  images: {
    unoptimized: true,
  },
  i18n: {
    locales: ['en', 'zh'],
    defaultLocale: 'en',
  },
}

module.exports = withNextra(nextConfig)
