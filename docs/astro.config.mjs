import { defineConfig } from 'astro/config';
import astroExpressiveCode from 'astro-expressive-code';

export default defineConfig({
  site: 'https://Basliel25.github.io',
  base: '/trafilo',
  outDir: './dist',
  server: { port: 4321, host: true },
  integrations: [
    astroExpressiveCode({
      themes: ['dracula', 'github-light'],
      themeCssSelector: (theme) => `[data-theme='${theme.type}']`,
      useThemedScrollbars: false,
      styleOverrides: {
        borderRadius: '0px',
        borderWidth: '2px',
      }
    })
  ]
});
