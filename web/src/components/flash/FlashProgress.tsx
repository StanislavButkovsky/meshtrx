'use client';

import type { FlashProgress as FlashProgressType } from '@/lib/flash-utils';
import { useLanguage } from '@/components/LanguageProvider';
import type { TranslationKey } from '@/lib/i18n';

interface Props {
  progress: FlashProgressType;
}

const stageKeys: Record<string, TranslationKey> = {
  idle: 'flash.stage.idle',
  connecting: 'flash.stage.connecting',
  erasing: 'flash.stage.erasing',
  flashing: 'flash.stage.flashing',
  verifying: 'flash.stage.verifying',
  done: 'flash.stage.done',
  error: 'flash.stage.error',
};

export default function FlashProgress({ progress }: Props) {
  const { t } = useLanguage();
  const isError = progress.stage === 'error';
  const isDone = progress.stage === 'done';

  return (
    <div className="space-y-3">
      <div className="flex items-center justify-between text-sm">
        <span className={`font-medium ${isError ? 'text-red-400' : isDone ? 'text-accent' : 'text-text-primary'}`}>
          {t(stageKeys[progress.stage])}
        </span>
        <span className="text-text-secondary">{progress.percent}%</span>
      </div>
      <div className="w-full h-2 bg-bg-hover rounded-full overflow-hidden">
        <div
          className={`h-full rounded-full transition-all duration-300 ${
            isError ? 'bg-red-500' : 'bg-accent'
          }`}
          style={{ width: `${progress.percent}%` }}
        />
      </div>
      <p className={`text-sm ${isError ? 'text-red-400' : 'text-text-secondary'}`}>
        {progress.message}
      </p>
    </div>
  );
}
