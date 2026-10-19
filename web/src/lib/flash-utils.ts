export interface FlashProgress {
  stage: 'idle' | 'connecting' | 'erasing' | 'flashing' | 'verifying' | 'done' | 'error';
  percent: number;
  message: string;
}

export function isWebSerialSupported(): boolean {
  return typeof navigator !== 'undefined' && 'serial' in navigator;
}

export function getBrowserName(): string {
  if (typeof navigator === 'undefined') return 'unknown';
  const ua = navigator.userAgent;
  if (ua.includes('Chrome') && !ua.includes('Edg')) return 'Chrome';
  if (ua.includes('Edg')) return 'Edge';
  if (ua.includes('Firefox')) return 'Firefox';
  if (ua.includes('Safari') && !ua.includes('Chrome')) return 'Safari';
  return 'unknown';
}

function toBinaryString(buffer: ArrayBuffer): string {
  const bytes = new Uint8Array(buffer);
  let str = '';
  for (let i = 0; i < bytes.length; i++) {
    str += String.fromCharCode(bytes[i]);
  }
  return str;
}

async function baud1200Reset(port: SerialPort): Promise<void> {
  await port.open({ baudRate: 1200 });
  await new Promise(r => setTimeout(r, 500));
  await port.close();
  await new Promise(r => setTimeout(r, 1000));
}

export async function requestPort(): Promise<SerialPort | null> {
  try {
    const port = await navigator.serial.requestPort({});
    return port;
  } catch {
    return null;
  }
}

// Что заливать на каждую плату. Раньше мастер умел только V3, а владельцам
// V4 предлагал esptool. У V4 своя прошивка (другой усилитель — другой вывод
// управления) и своя таблица разделов: памяти 16 МБ, а не 8. В заголовке
// загрузчика записан размер памяти; esptool.py правит его при записи
// (--flash-size 16MB), а esptool-js 0.5.7 этот шаг молча пропускает — читает
// двоичную строку через parseInt и решает, что это не образ. С загрузчиком
// «на 8 МБ» плата пишет «partition exceeds flash chip size» и не стартует.
// Поэтому для V4 лежит отдельный загрузчик, уже исправленный на 16 МБ
// (esptool merge_bin --flash_size 16MB, контрольная сумма пересчитана).
export interface FlashBoard {
  id: 'v3' | 'v4' | 'v43';
  label: string;
  firmware: string;
  bootloader: string;
  partitions: string;
  flash16MB: boolean;
}

export function flashBoards(links: { firmware: string; firmwareV4: string; firmwareV43: string }): FlashBoard[] {
  return [
    { id: 'v3', label: 'Heltec V3', firmware: links.firmware,
      bootloader: '/downloads/bootloader.bin', partitions: '/downloads/partitions.bin', flash16MB: false },
    { id: 'v4', label: 'Heltec V4 (rev 4.2)', firmware: links.firmwareV4,
      bootloader: '/downloads/bootloader-v4-16mb.bin', partitions: '/downloads/partitions-v4.bin', flash16MB: true },
    { id: 'v43', label: 'Heltec V4.3', firmware: links.firmwareV43,
      bootloader: '/downloads/bootloader-v4-16mb.bin', partitions: '/downloads/partitions-v4.bin', flash16MB: true },
  ];
}

export async function flashFirmware(
  port: SerialPort,
  board: FlashBoard,
  onProgress: (progress: FlashProgress) => void,
  onLog: (line: string) => void,
  base = ''
): Promise<void> {
  onProgress({ stage: 'connecting', percent: 0, message: 'Загрузка файлов прошивки...' });

  try {
    onLog(`> Плата: ${board.label}`);
    const [bootloaderRes, partitionsRes, bootApp0Res, firmwareRes] = await Promise.all([
      fetch(base + board.bootloader),
      fetch(base + board.partitions),
      fetch(base + '/downloads/boot_app0.bin'),
      fetch(base + board.firmware),
    ]);
    if (!bootloaderRes.ok) throw new Error(`Не удалось загрузить bootloader: ${bootloaderRes.status}`);
    if (!partitionsRes.ok) throw new Error(`Не удалось загрузить partitions: ${partitionsRes.status}`);
    if (!bootApp0Res.ok) throw new Error(`Не удалось загрузить boot_app0: ${bootApp0Res.status}`);
    if (!firmwareRes.ok) throw new Error(`Не удалось загрузить прошивку: ${firmwareRes.status}`);
    const bootloaderData = await bootloaderRes.arrayBuffer();
    const partitionsData = await partitionsRes.arrayBuffer();
    const bootApp0Data = await bootApp0Res.arrayBuffer();
    const firmwareData = await firmwareRes.arrayBuffer();
    onLog(`> Bootloader: ${bootloaderData.byteLength} байт`);
    onLog(`> Partitions: ${partitionsData.byteLength} байт`);
    onLog(`> Boot app0: ${bootApp0Data.byteLength} байт`);
    onLog(`> Firmware: ${firmwareData.byteLength} байт`);

    onLog('> Загрузка esptool...');
    const { ESPLoader, Transport } = await import('esptool-js');

    // 1200-baud reset trick — переводит ESP32-S3 (USB-CDC) в download mode.
    onProgress({ stage: 'connecting', percent: 5, message: 'Сброс в режим загрузки (1200 baud)...' });
    onLog('> 1200-baud reset для входа в download mode...');
    await baud1200Reset(port);

    const transport = new Transport(port, true);
    const esploader = new ESPLoader({
      transport,
      baudrate: 460800,
      romBaudrate: 115200,
      terminal: {
        clean: () => {},
        writeLine: (data: string) => onLog(data),
        write: (data: string) => onLog(data),
      },
    });

    onProgress({ stage: 'connecting', percent: 10, message: 'Подключение к ESP32-S3...' });
    onLog('> Подключение к ESP32...');
    const chipInfo = await esploader.main();
    onLog(`> Чип: ${chipInfo}`);
    if (board.flash16MB) {
      // Прошивка V4 на плату с 8 МБ не поместится — лучше сказать сразу,
      // чем залить и получить плату, которая не стартует
      const size = await esploader.getFlashSize();
      onLog(`> Память: ${size / 1024} МБ`);
      if (size > 0 && size < 16 * 1024) {
        throw new Error(`На плате ${size / 1024} МБ памяти, а у V4 — 16 МБ. Похоже, это V3: выберите V3 и прошейте снова.`);
      }
    }

    onProgress({ stage: 'flashing', percent: 20, message: 'Запись прошивки...' });
    onLog('> Запись прошивки...');

    const fileArray = [
      { data: toBinaryString(bootloaderData), address: 0x0 },
      { data: toBinaryString(partitionsData), address: 0x8000 },
      { data: toBinaryString(bootApp0Data), address: 0xe000 },
      { data: toBinaryString(firmwareData), address: 0x10000 },
    ];
    await esploader.writeFlash({
      fileArray,
      flashSize: 'keep',
      flashMode: 'keep',
      flashFreq: 'keep',
      eraseAll: false,
      compress: true,
      reportProgress: (_fileIndex: number, written: number, total: number) => {
        const pct = 20 + Math.round((written / total) * 70);
        onProgress({ stage: 'flashing', percent: pct, message: `Запись: ${written}/${total} байт` });
      },
    });

    onLog('> Перезагрузка устройства...');
    // DTR держит GPIO0 «внизу» с момента входа в загрузчик. Если не отпустить
    // его до сброса, плата с USB-JTAG (V4) перезагрузится обратно в режим
    // прошивки и будет молчать, хотя на экране «Готово».
    await transport.setDTR(false);
    await esploader.after('hard_reset');

    onProgress({ stage: 'done', percent: 100, message: 'Прошивка завершена!' });
    onLog('> Готово! Устройство перезагружается.');

    await transport.disconnect();
  } catch (error) {
    const msg = error instanceof Error ? error.message : 'Неизвестная ошибка';
    onProgress({ stage: 'error', percent: 0, message: msg });
    onLog(`> ОШИБКА: ${msg}`);
    throw error;
  }
}
