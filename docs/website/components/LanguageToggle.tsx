import React from 'react'
import { useRouter } from 'next/router'

const languages = [
  { code: 'en', label: 'EN', title: 'English' },
  { code: 'zh', label: '中文', title: '简体中文' }
]

// Map English section names to Chinese
const enToZhMap: Record<string, string> = {
  'introduction': '介绍',
  'getting-started': '入门指南',
  'core-concepts': '核心概念',
  'performance': '性能优化',
  'development': '开发指南',
  'reference': '参考文档'
}

const zhToEnMap: Record<string, string> = Object.fromEntries(
  Object.entries(enToZhMap).map(([en, zh]) => [zh, en])
)

export function LanguageToggle() {
  const router = useRouter()
  const currentPath = router.asPath

  // Determine current language from path
  const getCurrentLang = (): string => {
    if (currentPath.startsWith('/zh') || currentPath === '/zh') return 'zh'
    return 'en'
  }

  const currentLang = getCurrentLang()

  const switchLanguage = (targetLang: string) => {
    if (targetLang === currentLang) return

    let newPath = currentPath

    if (currentLang === 'en' && targetLang === 'zh') {
      // Switching from English to Chinese
      if (currentPath === '/' || currentPath === '') {
        newPath = '/zh'
      } else if (currentPath.startsWith('/en')) {
        // Replace /en with /zh
        newPath = currentPath.replace(/^\/en/, '/zh')
      } else {
        // Direct English pages - map section names
        const pathParts = currentPath.replace(/^\//, '').split('/')
        const firstPart = pathParts[0]

        if (enToZhMap[firstPart]) {
          pathParts[0] = enToZhMap[firstPart]
          newPath = '/zh/' + pathParts.join('/')
        } else {
          // Fallback to Chinese home
          newPath = '/zh'
        }
      }
    } else if (currentLang === 'zh' && targetLang === 'en') {
      // Switching from Chinese to English
      if (currentPath === '/zh' || currentPath === '/zh/') {
        newPath = '/en'
      } else {
        const pathAfterZh = currentPath.replace(/^\/zh\/?/, '').split('/')
        const firstPart = pathAfterZh[0]

        if (zhToEnMap[firstPart]) {
          pathAfterZh[0] = zhToEnMap[firstPart]
          newPath = '/en/' + pathAfterZh.join('/')
        } else {
          // Fallback to English home
          newPath = '/en'
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
