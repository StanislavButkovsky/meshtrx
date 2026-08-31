'use client';

import { useLanguage } from '@/components/LanguageProvider';

// Голосовой пакет MeshTRX в масштабе: ширина блока пропорциональна числу байт,
// поэтому видно главное — семь байт заголовка против тридцати двух байт речи.
// Числа те же, что в прошивке (packet.h, audio_codec.h).
const HEADER = [
  { bytes: 1, name: 'type' },
  { bytes: 1, name: 'chan' },
  { bytes: 1, name: 'seq' },
  { bytes: 1, name: 'flags' },
  { bytes: 1, name: 'ttl' },
  { bytes: 2, name: 'sender' },
];

const TOTAL_BYTES = 39;
const WIDTH = 960;
const SCALE = WIDTH / TOTAL_BYTES;

export default function PacketDiagram() {
  const { locale } = useLanguage();
  const ru = locale === 'ru';

  let x = 0;
  const headerCells = HEADER.map((field) => {
    const cell = { ...field, x, w: field.bytes * SCALE };
    x += cell.w;
    return cell;
  });
  const headerWidth = x;
  const frameWidth = (8 * SCALE);

  return (
    <div className="w-full overflow-x-auto pb-2">
      <svg
        viewBox="0 0 960 210"
        className="w-full"
        style={{ minWidth: 620 }}
        role="img"
        aria-label={
          ru
            ? 'Схема голосового пакета MeshTRX: 7 байт заголовка и 32 байта речи, четыре кадра Codec2 по 8 байт'
            : 'MeshTRX voice packet layout: 7 bytes of header and 32 bytes of speech, four 8-byte Codec2 frames'
        }
      >
        {/* Заголовок */}
        {headerCells.map((cell) => (
          <g key={cell.name}>
            <rect
              x={cell.x + 1}
              y={40}
              width={cell.w - 2}
              height={46}
              rx="3"
              fill="#60a5fa15"
              stroke="#60a5fa"
              strokeWidth="1.5"
            />
            {/* Однобайтовая ячейка — это меньше 25 px по ширине, и подпись в
                строку в неё не помещается: соседние названия налезают друг на
                друга. Развёрнутые на бок, они читаются и не спорят с масштабом,
                ради которого схема и рисуется. */}
            <text
              x={cell.x + cell.w / 2}
              y={63}
              textAnchor="middle"
              fill="#60a5fa"
              fontSize="10"
              fontFamily="monospace"
              transform={`rotate(-90 ${cell.x + cell.w / 2} 63)`}
            >
              {cell.name}
            </text>
          </g>
        ))}

        {/* Полезная нагрузка: четыре кадра Codec2 */}
        {[0, 1, 2, 3].map((i) => (
          <g key={i}>
            <rect
              x={headerWidth + i * frameWidth + 1}
              y={40}
              width={frameWidth - 2}
              height={46}
              rx="3"
              fill="#4ade8015"
              stroke="#4ade80"
              strokeWidth="1.5"
              className="animate-pulse-line"
            />
            <text
              x={headerWidth + i * frameWidth + frameWidth / 2}
              y={62}
              textAnchor="middle"
              fill="#4ade80"
              fontSize="11"
              fontFamily="monospace"
            >
              Codec2
            </text>
            <text
              x={headerWidth + i * frameWidth + frameWidth / 2}
              y={77}
              textAnchor="middle"
              fill="#4ade80"
              fontSize="10"
              fontFamily="monospace"
              opacity="0.75"
            >
              8 B · 20 ms
            </text>
          </g>
        ))}

        {/* Скобка заголовка */}
        <path
          d={`M0 96 L0 104 L${headerWidth} 104 L${headerWidth} 96`}
          fill="none"
          stroke="#60a5fa"
          strokeWidth="1.5"
          opacity="0.6"
        />
        <text x={headerWidth / 2} y={124} textAnchor="middle" fill="#60a5fa" fontSize="12" fontFamily="monospace">
          {ru ? '7 байт' : '7 bytes'}
        </text>
        <text x={headerWidth / 2} y={140} textAnchor="middle" fill="#a3a3a3" fontSize="11" fontFamily="monospace">
          {ru ? 'заголовок' : 'header'}
        </text>

        {/* Скобка полезной нагрузки */}
        <path
          d={`M${headerWidth} 96 L${headerWidth} 104 L${WIDTH} 104 L${WIDTH} 96`}
          fill="none"
          stroke="#4ade80"
          strokeWidth="1.5"
          opacity="0.6"
        />
        <text
          x={headerWidth + (WIDTH - headerWidth) / 2}
          y={124}
          textAnchor="middle"
          fill="#4ade80"
          fontSize="12"
          fontFamily="monospace"
        >
          {ru ? '32 байта — 80 мс речи' : '32 bytes — 80 ms of speech'}
        </text>
        <text
          x={headerWidth + (WIDTH - headerWidth) / 2}
          y={140}
          textAnchor="middle"
          fill="#a3a3a3"
          fontSize="11"
          fontFamily="monospace"
        >
          {ru ? 'четыре кадра Codec2 3200' : 'four Codec2 3200 frames'}
        </text>

        {/* Итог */}
        <line x1="0" y1="168" x2={WIDTH} y2="168" stroke="#333333" strokeWidth="1" />
        <text x={WIDTH / 2} y={192} textAnchor="middle" fill="#f5f5f5" fontSize="13" fontFamily="monospace">
          {ru
            ? '39 байт в эфире · MeshTRX · LoRa SF7 / BW 250 кГц / CR 4/7'
            : '39 bytes on air · MeshTRX · LoRa SF7 / BW 250 kHz / CR 4/7'}
        </text>

        {/* Подпись оси сверху */}
        <text x="0" y={26} fill="#a3a3a3" fontSize="11" fontFamily="monospace">
          {ru ? 'байт 0' : 'byte 0'}
        </text>
        <text x={WIDTH} y={26} textAnchor="end" fill="#a3a3a3" fontSize="11" fontFamily="monospace">
          {ru ? 'байт 38' : 'byte 38'}
        </text>
      </svg>
    </div>
  );
}
