import { Html, Head, Main, NextScript } from 'next/document'

export default function Document() {
  const jsonLd = {
    '@context': 'https://schema.org',
    '@type': 'SoftwareApplication',
    name: 'fq-compressor',
    description: 'High-performance FASTQ compression tool with Assembly-based Compression (ABC) and O(1) random access',
    applicationCategory: 'UtilitiesApplication',
    operatingSystem: 'Linux',
    offers: {
      '@type': 'Offer',
      price: '0',
      priceCurrency: 'USD',
    },
    author: {
      '@type': 'Organization',
      name: 'LessUp',
      url: 'https://github.com/LessUp',
    },
    softwareVersion: '0.2.0',
    downloadUrl: 'https://github.com/LessUp/fq-compressor/releases',
    codeRepository: 'https://github.com/LessUp/fq-compressor',
    license: 'https://opensource.org/licenses/MIT',
    programmingLanguage: 'C',
    runtimePlatform: 'Linux x86_64',
  }

  return (
    <Html lang="en">
      <Head>
        <script
          type="application/ld+json"
          dangerouslySetInnerHTML={{ __html: JSON.stringify(jsonLd) }}
        />
      </Head>
      <body>
        <Main />
        <NextScript />
      </body>
    </Html>
  )
}
