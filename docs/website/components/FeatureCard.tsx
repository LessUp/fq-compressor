import React from 'react'
import { ArrowRight, LucideIcon } from 'lucide-react'

interface FeatureCardProps {
  icon: LucideIcon
  iconColor: string
  title: string
  description: string
  href: string
}

export function FeatureCard({ icon: Icon, iconColor, title, description, href }: FeatureCardProps) {
  return (
    <a 
      href={href}
      className="feature-card group block"
    >
      <div className={`w-14 h-14 rounded-xl ${iconColor} bg-opacity-10 flex items-center justify-center mb-4 transition-transform duration-300 group-hover:scale-110`}>
        <Icon className={`w-7 h-7 ${iconColor.replace('bg-', 'text-')}`} />
      </div>
      <h3 className="text-lg font-semibold mb-2 flex items-center gap-2">
        {title}
        <ArrowRight className="w-4 h-4 opacity-0 -translate-x-2 transition-all duration-300 group-hover:opacity-100 group-hover:translate-x-0" />
      </h3>
      <p className="text-gray-600 dark:text-gray-400 text-sm leading-relaxed">
        {description}
      </p>
    </a>
  )
}

interface FeatureGridProps {
  title: string
  features: Array<{
    icon: LucideIcon
    iconColor: string
    title: string
    description: string
    href: string
  }>
}

export function FeatureGrid({ title, features }: FeatureGridProps) {
  return (
    <div className="container mx-auto px-4 py-16">
      <h2 className="text-3xl md:text-4xl font-bold text-center mb-4">{title}</h2>
      <div className="w-20 h-1 bg-gradient-to-r from-primary-600 to-purple-600 mx-auto mb-12 rounded-full" />
      
      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6 max-w-6xl mx-auto">
        {features.map((feature, index) => (
          <FeatureCard key={index} {...feature} />
        ))}
      </div>
    </div>
  )
}
