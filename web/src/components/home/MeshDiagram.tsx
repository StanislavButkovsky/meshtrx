'use client';

export default function MeshDiagram() {
  const nodes = [
    { x: 60, y: 100, label: 'Phone', type: 'phone' },
    { x: 200, y: 100, label: 'BLE', type: 'ble' },
    { x: 340, y: 60, label: 'Heltec', type: 'node' },
    { x: 480, y: 100, label: 'LoRa', type: 'lora' },
    { x: 620, y: 60, label: 'Heltec', type: 'node' },
    { x: 760, y: 100, label: 'BLE', type: 'ble' },
    { x: 900, y: 100, label: 'Phone', type: 'phone' },
  ];

  const links = [
    { from: 0, to: 1, type: 'ble' },
    { from: 1, to: 2, type: 'ble' },
    { from: 2, to: 3, type: 'lora' },
    { from: 3, to: 4, type: 'lora' },
    { from: 4, to: 5, type: 'ble' },
    { from: 5, to: 6, type: 'ble' },
  ];

  return (
    <div className="w-full overflow-x-auto pb-4">
      <svg viewBox="0 0 960 180" className="w-full max-w-4xl mx-auto" style={{ minWidth: 600 }}>
        {links.map((link, i) => (
          <line
            key={i}
            x1={nodes[link.from].x}
            y1={nodes[link.from].y}
            x2={nodes[link.to].x}
            y2={nodes[link.to].y}
            stroke={link.type === 'lora' ? '#4ade80' : '#60a5fa'}
            strokeWidth="2"
            strokeDasharray={link.type === 'lora' ? '8 4' : 'none'}
            className={link.type === 'lora' ? 'animate-dash' : ''}
            opacity="0.6"
          />
        ))}
        {nodes.map((node, i) => (
          <g key={i}>
            <circle
              cx={node.x}
              cy={node.y}
              r={node.type === 'node' ? 24 : 18}
              fill={
                node.type === 'node' ? '#4ade8020' :
                node.type === 'phone' ? '#60a5fa15' :
                '#a78bfa15'
              }
              stroke={
                node.type === 'node' ? '#4ade80' :
                node.type === 'phone' ? '#60a5fa' :
                '#a78bfa'
              }
              strokeWidth="1.5"
              className={node.type === 'node' ? 'animate-pulse-line' : ''}
            />
            <text
              x={node.x}
              y={node.y + 4}
              textAnchor="middle"
              fill={
                node.type === 'node' ? '#4ade80' :
                node.type === 'phone' ? '#60a5fa' :
                '#a78bfa'
              }
              fontSize="10"
              fontFamily="monospace"
            >
              {node.type === 'phone' ? '📱' : node.type === 'node' ? '📡' : '📶'}
            </text>
            <text
              x={node.x}
              y={node.y + 40}
              textAnchor="middle"
              fill="#a3a3a3"
              fontSize="11"
              fontFamily="monospace"
            >
              {node.label}
            </text>
          </g>
        ))}
        {/* Legend */}
        <g transform="translate(340, 150)">
          <line x1="0" y1="0" x2="30" y2="0" stroke="#60a5fa" strokeWidth="2" />
          <text x="35" y="4" fill="#a3a3a3" fontSize="10" fontFamily="monospace">BLE</text>
          <line x1="80" y1="0" x2="110" y2="0" stroke="#4ade80" strokeWidth="2" strokeDasharray="8 4" />
          <text x="115" y="4" fill="#a3a3a3" fontSize="10" fontFamily="monospace">LoRa</text>
        </g>
      </svg>
    </div>
  );
}
