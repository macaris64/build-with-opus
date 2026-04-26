import { useMarsStore, VEHICLE_COLOR } from '../store/marsStore'
import type { CommEvent } from '../types/mars'

function formatTs(ms: number): string {
  const d = new Date(ms)
  const hh = d.getHours().toString().padStart(2, '0')
  const mm = d.getMinutes().toString().padStart(2, '0')
  const ss = d.getSeconds().toString().padStart(2, '0')
  const mmm = d.getMilliseconds().toString().padStart(3, '0')
  return `${hh}:${mm}:${ss}.${mmm}`
}

function apidHex(apid: number): string {
  return `0x${apid.toString(16).toUpperCase().padStart(3, '0')}`
}

function PacketRow({ evt }: { evt: CommEvent }) {
  const vehicles = useMarsStore((s) => s.vehicles)
  const fromV = vehicles.find((v) => v.id === evt.fromId)
  const toV = vehicles.find((v) => v.id === evt.toId)
  const color = fromV ? VEHICLE_COLOR[fromV.type] : '#888'
  const toColor = toV ? VEHICLE_COLOR[toV.type] : '#888'

  const fieldStr = Object.entries(evt.decodedFields)
    .map(([k, v]) => `${k}=${v}`)
    .join(' · ')

  return (
    <div
      style={{
        borderBottom: '1px solid #1a2a3a',
        padding: '8px 10px',
        fontFamily: '"JetBrains Mono", ui-monospace, monospace',
        fontSize: '10px',
      }}
    >
      <div style={{ color: '#4b5563', marginBottom: '2px' }}>{formatTs(evt.timestamp)}</div>
      <div style={{ display: 'flex', alignItems: 'center', gap: '6px', marginBottom: '2px' }}>
        <span style={{ color, fontWeight: 700 }}>{fromV?.label ?? evt.fromId}</span>
        <span style={{ color: '#374151' }}>→</span>
        <span style={{ color: toColor, fontWeight: 700 }}>{toV?.label ?? evt.toId}</span>
      </div>
      <div style={{ color: '#22d3ee', marginBottom: '2px' }}>
        {apidHex(evt.apid)} <span style={{ color: '#6b7280' }}>inst={fromV?.instanceId ?? '?'}</span>{' '}
        <span style={{ color: '#9ca3af' }}>{evt.apidName}</span>
      </div>
      <div style={{ color: '#6b7280', lineHeight: 1.5 }}>{fieldStr}</div>
    </div>
  )
}

export function MarsPacketPanel() {
  const packetLog = useMarsStore((s) => s.packetLog)

  return (
    <div className="flex flex-col h-full overflow-hidden bg-gray-950">
      {/* Header */}
      <div className="flex items-center justify-between px-4 py-2 border-b border-gray-800 shrink-0">
        <span className="text-xs font-bold tracking-widest text-cyan-400">CCSDS PACKET LOG</span>
        <span className="text-xs text-gray-600 font-mono">{packetLog.length} pkts</span>
      </div>

      {/* Legend */}
      <div className="flex gap-4 px-4 py-2 border-b border-gray-800 shrink-0">
        {([['drone', '#38bdf8', 'DRONE'], ['land', '#f97316', 'ROVER'], ['cryobot', '#a78bfa', 'CRYOBOT']] as const).map(([, color, label]) => (
          <div key={label} className="flex items-center gap-1.5">
            <span className="w-2 h-2 rounded-full inline-block" style={{ backgroundColor: color }} />
            <span className="text-xs text-gray-500">{label}</span>
          </div>
        ))}
      </div>

      {/* Packet list — newest at top */}
      <div className="flex-1 overflow-y-auto">
        {packetLog.length === 0 ? (
          <div className="flex items-center justify-center h-32 text-xs text-gray-600">
            Awaiting inter-vehicle transmissions…
          </div>
        ) : (
          packetLog.map((evt) => <PacketRow key={evt.id} evt={evt} />)
        )}
      </div>
    </div>
  )
}
