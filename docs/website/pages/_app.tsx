import type { AppProps } from 'next/app'
import { Inter, JetBrains_Mono } from 'next/font/google'
import { BackToTop } from '../components/BackToTop'
import { ReadingProgress } from '../components/ReadingProgress'
import '../styles/globals.css'

const inter = Inter({
  subsets: ['latin'],
  display: 'swap',
  variable: '--font-inter',
})

const jetbrainsMono = JetBrains_Mono({
  subsets: ['latin'],
  display: 'swap',
  variable: '--font-mono',
})

export default function App({ Component, pageProps }: AppProps) {
  return (
    <div className={`${inter.variable} ${jetbrainsMono.variable}`}>
      <ReadingProgress />
      <Component {...pageProps} />
      <BackToTop />
    </div>
  )
}
