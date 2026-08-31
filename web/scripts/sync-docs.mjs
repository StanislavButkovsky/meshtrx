// Собирает содержимое страницы /docs/ из руководства, которое лежит в
// репозитории, — чтобы сайт и репозиторий не расходились.
//
// Раньше на сайте жила своя, отдельно написанная копия руководства. Она
// отстала молча: в ней остался только Heltec V3, не было ни предела в десять
// секунд, ни голосовой активации, ни настольного клиента, зато была
// маршрутизация «до трёх хопов», которой у нас никогда не было. Сверять две
// копии руками никто не будет, поэтому копия здесь ровно одна, а страница
// собирается из неё.
//
// Запускается сам перед каждой сборкой (npm run build → prebuild).

import { readFileSync, writeFileSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const repo = resolve(here, '..', '..');
const out = resolve(here, '..', 'src', 'lib', 'docs-content.ts');

const GITHUB = 'https://github.com/StanislavButkovsky/meshtrx/blob/master/';

function load(file) {
  return readFileSync(resolve(repo, 'docs', file), 'utf-8');
}

// Ссылки вида ../desktop/README.md работают в репозитории и ломаются на сайте:
// уводят на несуществующую страницу. На сайте они должны вести в GitHub.
function fixLinks(md) {
  return md.replace(/\]\(\.\.\/([^)]+)\)/g, (_, path) => `](${GITHUB}${path})`);
}

// Текст едет внутрь обратных кавычек, поэтому экранируем то, что их закрывает
// или запускает подстановку.
function escape(md) {
  return md.replace(/\\/g, '\\\\').replace(/`/g, '\\`').replace(/\$\{/g, '\\${');
}

function headings(md) {
  return (md.match(/^## /gm) || []).length;
}

const ru = load('USER_GUIDE.md');
const en = load('USER_GUIDE.en.md');

// Расхождение в числе разделов — верный признак, что перевод отстал от
// оригинала. Сборку не роняем: английская страница с недостающим разделом
// лучше, чем не вышедший релиз, — но молчать об этом нельзя.
if (headings(ru) !== headings(en)) {
  console.warn(
    `[sync-docs] разделов в USER_GUIDE.md — ${headings(ru)}, ` +
    `в USER_GUIDE.en.md — ${headings(en)}: перевод отстал, обновите его`
  );
}

const banner = `// СГЕНЕРИРОВАНО автоматически из docs/USER_GUIDE.md и docs/USER_GUIDE.en.md.
// Не правьте этот файл: правки затрёт следующая сборка. Правьте руководство
// в docs/ — скрипт web/scripts/sync-docs.mjs перенесёт их сюда.
`;

writeFileSync(
  out,
  `${banner}\nexport const USER_GUIDE = \`${escape(fixLinks(ru))}\`;\n\n` +
  `export const USER_GUIDE_EN = \`${escape(fixLinks(en))}\`;\n`,
  'utf-8'
);

console.log(`[sync-docs] страница /docs/ собрана из руководства: ` +
  `${headings(ru)} разделов, ${Math.round(ru.length / 1024)} КБ`);
