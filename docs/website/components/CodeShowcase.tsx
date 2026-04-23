import React, { useState } from 'react'
import { Check, Copy, Terminal } from 'lucide-react'

interface CodeTab {
  label: string
  code: string
}

interface CodeShowcaseProps {
  title: string
  tabs: CodeTab[]
}

export function CodeShowcase({ title, tabs }: CodeShowcaseProps) {
  const [activeTab, setActiveTab] = useState(0)
  const [copied, setCopied] = useState(false)

  const handleCopy = async () => {
    await navigator.clipboard.writeText(tabs[activeTab].code)
    setCopied(true)
    setTimeout(() => setCopied(false), 2000)
  }

  return (
    <div className="container mx-auto px-4 py-16">
      <h2 className="text-3xl md:text-4xl font-bold text-center mb-4">{title}</h2>
      <div className="w-20 h-1 bg-gradient-to-r from-primary-600 to-purple-600 mx-auto mb-12 rounded-full" />
      
      <div className="max-w-4xl mx-auto">
        <div className="code-block">
          {/* Tab Header */}
          <div className="flex items-center justify-between border-b border-gray-200 dark:border-gray-700 bg-gray-100 dark:bg-gray-800 px-4 py-2">
            <div className="flex items-center gap-1">
              <div className="w-3 h-3 rounded-full bg-red-400" />
              <div className="w-3 h-3 rounded-full bg-yellow-400" />
              <div className="w-3 h-3 rounded-full bg-green-400" />
              <Terminal className="w-4 h-4 ml-3 text-gray-500" />
            </div>
            <div className="flex gap-1">
              {tabs.map((tab, index) => (
                <button
                  key={index}
                  onClick={() => setActiveTab(index)}
                  className={`px-4 py-1.5 text-sm font-medium rounded-t-lg transition-colors ${
                    activeTab === index
                      ? 'bg-white dark:bg-gray-900 text-primary-600 dark:text-primary-400 border-t border-x border-gray-200 dark:border-gray-700'
                      : 'text-gray-500 hover:text-gray-700 dark:hover:text-gray-300'
                  }`}
                >
                  {tab.label}
                </button>
              ))}
            </div>
            <button
              onClick={handleCopy}
              className="p-2 hover:bg-gray-200 dark:hover:bg-gray-700 rounded-lg transition-colors"
              title="Copy to clipboard"
            >
              {copied ? (
                <Check className="w-4 h-4 text-green-500" />
              ) : (
                <Copy className="w-4 h-4 text-gray-500" />
              )}
            </button>
          </div>
          
          {/* Code Content */}
          <div className="p-4 overflow-x-auto">
            <pre className="!bg-transparent !border-0">
              <code className="text-sm">{tabs[activeTab].code}</code>
            </pre>
          </div>
        </div>
      </div>
    </div>
  )
}
