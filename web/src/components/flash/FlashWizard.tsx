'use client';

import { useState, useRef } from 'react';
import { isWebSerialSupported, requestPort, flashFirmware, type FlashProgress as FlashProgressType } from '@/lib/flash-utils';
import { DOWNLOAD_LINKS, VERSION } from '@/lib/constants';
import { useLanguage } from '@/components/LanguageProvider';
import type { TranslationKey } from '@/lib/i18n';
import BrowserCheck from './BrowserCheck';
import FlashProgress from './FlashProgress';

const STEP_KEYS: TranslationKey[] = ['flash.step1', 'flash.step2', 'flash.step3'];

export default function FlashWizard() {
  const { t } = useLanguage();
  const [port, setPort] = useState<SerialPort | null>(null);
  const [progress, setProgress] = useState<FlashProgressType>({
    stage: 'idle',
    percent: 0,
    message: '',
  });
  const [logs, setLogs] = useState<string[]>([]);
  const [flashing, setFlashing] = useState(false);
  const logRef = useRef<HTMLDivElement>(null);

  const idleMessage = !port ? t('flash.idle') : t('flash.port.ok');

  const addLog = (line: string) => {
    setLogs((prev) => [...prev, line]);
    setTimeout(() => {
      if (logRef.current) {
        logRef.current.scrollTop = logRef.current.scrollHeight;
      }
    }, 50);
  };

  const handleConnect = async () => {
    if (!isWebSerialSupported()) return;
    const selectedPort = await requestPort();
    if (selectedPort) {
      setPort(selectedPort);
      addLog('> Port selected');
      setProgress({ stage: 'idle', percent: 0, message: t('flash.port.ok') });
    }
  };

  const handleFlash = async () => {
    if (!port) return;
    setFlashing(true);
    setLogs([]);
    try {
      await flashFirmware(port, DOWNLOAD_LINKS.firmware, setProgress, addLog);
    } catch {
      // Error handled in flashFirmware
    } finally {
      setFlashing(false);
    }
  };

  const step = !port ? 1 : progress.stage === 'idle' ? 2 : 3;

  return (
    <div className="space-y-6">
      <BrowserCheck />

      <div className="flex items-center gap-4">
        {STEP_KEYS.map((key, i) => (
          <div key={i} className="flex items-center gap-2">
            <div
              className={`w-8 h-8 rounded-full flex items-center justify-center text-sm font-bold border-2 ${
                step >= i + 1
                  ? 'border-accent text-accent bg-accent/10'
                  : 'border-border text-text-secondary'
              }`}
            >
              {i + 1}
            </div>
            <span className={`text-sm ${step >= i + 1 ? 'text-text-primary' : 'text-text-secondary'}`}>
              {t(key)}
            </span>
            {i < 2 && <div className="w-8 h-px bg-border" />}
          </div>
        ))}
      </div>

      <div className="p-6 rounded-xl bg-bg-card border border-border space-y-4">
        <div className="flex items-center justify-between">
          <div>
            <h3 className="font-semibold text-text-primary">Firmware v{VERSION.firmware}</h3>
            <p className="text-sm text-text-secondary">Heltec WiFi LoRa 32 V3 (ESP32-S3)</p>
          </div>
          <div className="flex gap-3">
            {!port && (
              <button
                onClick={handleConnect}
                disabled={!isWebSerialSupported()}
                className="px-4 py-2 bg-accent text-bg font-semibold rounded-lg hover:bg-accent-dim transition-colors disabled:opacity-50 disabled:cursor-not-allowed"
              >
                {t('flash.select')}
              </button>
            )}
            {port && (
              <button
                onClick={handleFlash}
                disabled={flashing}
                className="px-4 py-2 bg-accent text-bg font-semibold rounded-lg hover:bg-accent-dim transition-colors disabled:opacity-50 disabled:cursor-not-allowed"
              >
                {flashing ? t('flash.btn.busy') : t('flash.btn')}
              </button>
            )}
          </div>
        </div>

        <FlashProgress progress={{
          ...progress,
          message: progress.message || idleMessage,
        }} />

        {port && progress.stage === 'idle' && (
          <div className="p-3 rounded-lg bg-yellow-500/15 border border-yellow-500/40">
            <p className="text-sm font-semibold text-yellow-400">
              {t('flash.port.ok')}
            </p>
          </div>
        )}
      </div>

      {logs.length > 0 && (
        <div className="rounded-xl bg-bg-card border border-border overflow-hidden">
          <div className="px-4 py-2 border-b border-border flex items-center gap-2">
            <div className="w-3 h-3 rounded-full bg-red-500/50" />
            <div className="w-3 h-3 rounded-full bg-yellow-500/50" />
            <div className="w-3 h-3 rounded-full bg-green-500/50" />
            <span className="text-xs text-text-secondary ml-2">Terminal</span>
          </div>
          <div ref={logRef} className="p-4 max-h-64 overflow-y-auto font-mono text-xs text-text-secondary space-y-0.5">
            {logs.map((line, i) => (
              <div key={i} className={line.includes('ERROR') || line.includes('ОШИБКА') ? 'text-red-400' : line.includes('Done') || line.includes('Готово') ? 'text-accent' : ''}>
                {line}
              </div>
            ))}
          </div>
        </div>
      )}
    </div>
  );
}
