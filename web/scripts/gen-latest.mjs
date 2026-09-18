// Файл с текущими версиями для приложения: оно спрашивает его раз в сутки и
// показывает человеку, что вышло новое, — раньше за каждым выпуском нужно было
// идти на сайт руками, а выпусков бывает несколько в неделю.
//
// Собирается из тех же констант, что и страница загрузки, чтобы номер версии
// нельзя было обновить в одном месте и забыть в другом.
import { readFileSync, writeFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';

const here = dirname(fileURLToPath(import.meta.url));
const root = join(here, '..');

const constants = readFileSync(join(root, 'src/lib/constants.ts'), 'utf8');
const pick = (key) => {
  const m = constants.match(new RegExp(`${key}:\\s*'([^']+)'`));
  if (!m) throw new Error(`не нашёл ${key} в constants.ts`);
  return m[1];
};

const gradle = readFileSync(join(root, '../android/MeshTRX/app/build.gradle'), 'utf8');
const codeMatch = gradle.match(/versionCode\s+(\d+)/);
if (!codeMatch) throw new Error('не нашёл versionCode в build.gradle');

const app = pick('app');
const firmware = pick('firmware');

const latest = {
  app: {
    version: app,
    // Номер сборки: по нему приложение и сравнивает, есть ли что-то новее.
    // Сравнивать строки вида «4.4.9» и «4.4.13» нельзя — вторая «меньше».
    code: Number(codeMatch[1]),
    url: `https://meshtrx.ru/downloads/meshtrx-${app}.apk`,
  },
  firmware: {
    version: firmware,
    v3: `https://meshtrx.ru/downloads/firmware-v3-${firmware}.bin`,
    v4: `https://meshtrx.ru/downloads/firmware-v4-${firmware}.bin`,
    v43: `https://meshtrx.ru/downloads/firmware-v4.3-${firmware}.bin`,
  },
  date: pick('date'),
  changelog: 'https://meshtrx.ru/download/',
};

writeFileSync(join(root, 'public/latest.json'), JSON.stringify(latest, null, 2) + '\n');
console.log(`latest.json: приложение ${app} (сборка ${latest.app.code}), прошивка ${firmware}`);
