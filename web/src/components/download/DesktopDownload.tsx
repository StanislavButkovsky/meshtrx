'use client';

import { VERSION_DESKTOP, DESKTOP_LINKS } from '@/lib/constants';
import { useLanguage } from '@/components/LanguageProvider';

// Настольный клиент: то же, что приложение на телефоне, но на компьютере —
// подключается к рации по Bluetooth.
export default function DesktopDownload() {
  const { locale } = useLanguage();
  const ru = locale === 'ru';
  const btn = 'inline-flex items-center gap-2 px-5 py-2.5 bg-bg border border-accent/40 text-accent font-semibold rounded-lg hover:bg-accent/10 transition-colors';
  return (
    <div id="desktop" className="p-6 rounded-xl bg-bg-card border border-border">
      <div className="flex items-start gap-4">
        <div className="w-16 h-16 rounded-xl bg-accent/10 border border-accent/30 flex items-center justify-center text-2xl flex-shrink-0">
          💻
        </div>
        <div className="flex-1">
          <h2 className="text-xl font-bold text-text-primary mb-1">
            {ru ? 'Программа для компьютера' : 'Desktop app'}
          </h2>
          <div className="flex items-center gap-3 text-sm text-text-secondary mb-3">
            <span className="px-2 py-0.5 rounded bg-accent/10 text-accent text-xs font-medium">
              v{VERSION_DESKTOP.app}
            </span>
            <span>{VERSION_DESKTOP.date}</span>
          </div>
          <p className="text-sm text-text-secondary mb-4">
            {ru
              ? 'Голос, сообщения и голосовые, файлы, вызовы, радар, карта и шифрование — как в телефоне. Компьютер подключается к рации по Bluetooth.'
              : 'Voice, text and voice messages, files, calls, radar, map and encryption — same as the phone app. The computer connects to the radio over Bluetooth.'}
          </p>
          <div className="flex flex-wrap gap-3 mb-4">
            <a href={DESKTOP_LINKS.windows} className={btn}>Windows</a>
            <a href={DESKTOP_LINKS.macArm} className={btn}>macOS · Apple M</a>
            <a href={DESKTOP_LINKS.macIntel} className={btn}>macOS · Intel</a>
            <a href={DESKTOP_LINKS.linux} className={btn}>Linux</a>
          </div>
          <ul className="text-xs text-text-secondary space-y-1 list-disc list-inside">
            {ru ? (
              <>
                <li>Windows: при первом запуске SmartScreen предупредит о неизвестном издателе — «Подробнее» → «Выполнить в любом случае».</li>
                <li>macOS: распакуйте архив, затем правой кнопкой по MeshTRX → «Открыть» → «Открыть»; разрешите Bluetooth и микрофон.</li>
                <li>Linux: распакуйте и запустите MeshTRX; нужен включённый Bluetooth (BlueZ).</li>
              </>
            ) : (
              <>
                <li>Windows: on first launch SmartScreen warns about an unknown publisher — “More info” → “Run anyway”.</li>
                <li>macOS: unzip, then right-click MeshTRX → Open → Open; allow Bluetooth and microphone access.</li>
                <li>Linux: unpack and run MeshTRX; Bluetooth (BlueZ) must be on.</li>
              </>
            )}
          </ul>
        </div>
      </div>
    </div>
  );
}
