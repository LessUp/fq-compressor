import React from 'react'
import { useRouter } from 'next/router'

const languages = [
  { code: 'en', label: 'EN', title: 'English' },
  { code: 'zh', label: '中文', title: '简体中文' }
]

export function LanguageToggle() {
  const router = useRouter()
  const currentPath = router.asPath

  // Determine current language from path
  const getCurrentLang = () => {
    if (currentPath.includes('/zh')) return 'zh'
    return 'en'
  }

  const currentLang = getCurrentLang()

  const switchLanguage = (targetLang: string) => {
    if (targetLang === currentLang) return

    // Calculate new path
    let newPath = currentPath

    if (currentLang === 'en' && targetLang === 'zh') {
      // Switching from English to Chinese
      // Handle root path and /en paths
      if (currentPath === '/fq-compressor' || currentPath === '/fq-compressor/') {
        newPath = '/fq-compressor/zh'
      } else if (currentPath.startsWith('/fq-compressor/en')) {
        newPath = currentPath.replace('/fq-compressor/en', '/fq-compressor/zh')
      } else {
        // Direct English pages (like /fq-compressor/intro)
        // Try to find Chinese equivalent
        const pathParts = currentPath.replace('/fq-compressor/', '').split('/')
        const enPages = ['introduction', 'getting-started', 'core-concepts', 'performance', 'development', 'reference']
        const zhPages = ['介绍', '入门指南', '核心概念', '性能优化', '开发指南', '参考文档']

        const firstPart = pathParts[0]
        const enIndex = enPages.indexOf(firstPart)
        if (enIndex !== -1) {
          pathParts[0] = zhPages[enIndex]
          newPath = '/fq-compressor/zh/' + pathParts.join('/')
        } else {
          newPath = '/fq-compressor/zh'
        }
      }
    } else if (currentLang === 'zh' && targetLang === 'en') {
      // Switching from Chinese to English
      if (currentPath.startsWith('/fq-compressor/zh')) {
        const zhPages = ['介绍', '入门指南', '核心概念', '性能优化', '开发指南', '参考文档']
        const enPages = ['introduction', 'getting-started', 'core-concepts', 'performance', 'development', 'reference']

        const pathAfterZh = currentPath.replace('/fq-compressor/zh/', '').split('/')
        const firstPart = pathAfterZh[0]
        const zhIndex = zhPages.indexOf(firstPart)

        if (zhIndex !== -1) {
          pathAfterZh[0] = enPages[zhIndex]
          newPath = '/fq-compressor/en/' + pathAfterZh.join('/')
        } else {
          newPath = currentPath.replace('/fq-compressor/zh', '/fq-compressor/en')
        }
      }
    }

    router.push(newPath)
  }

  return (
    <div className="flex items-center gap-1 rounded-lg bg-gray-100 dark:bg-gray-800 p-0.5">
      {languages.map((lang) => (
        <button
          key={lang.code}
          onClick={() => switchLanguage(lang.code)}
          title={lang.title}
          className={`px-2 py-1 text-xs font-medium rounded-md transition-colors ${
            currentLang === lang.code
              ? 'bg-white dark:bg-gray-700 text-primary-600 dark:text-primary-400 shadow-sm'
              : 'text-gray-600 dark:text-gray-400 hover:text-gray-900 dark:hover:text-gray-200'
          }`}
        >
          {lang.label}
        </button>
      ))}
    </div>
  )
}
