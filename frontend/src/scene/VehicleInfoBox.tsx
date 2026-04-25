import { Html } from '@react-three/drei'
import { useTelemetryStore } from '../store/telemetryStore'
import { useUiStore } from '../store/uiStore'
import { decodeHkFrame } from '../lib/hkDecoder'

const VARIANT_COLOR: Record<string, string> = {
  land: '#f97316',
  uav: '#38bdf8',
  cryobot: '#a78bfa',
}

const VARIANT_ICON: Record<string, string> = {
  land: '⛽',
  uav: '🛸',
  cryobot: '🔩',
}

const VARIANT_ROLE: Record<string, string> = {
  land: 'SURFACE ROVER',
  uav: 'AERIAL DRONE',
  cryobot: 'ICE DRILLER',
}

interface RowProps {
  label: string
  value: string
  valueColor?: string
}

function Row({ label, value, valueColor }: RowProps) {
  return (
    <div style={{ display: 'flex', justifyContent: 'space-between', gap: '10px', lineHeight: '1.5' }}>
      <span style={{ color: '#6b7280', flexShrink: 0 }}>{label}</span>
      <span
        style={{
          color: valueColor ?? '#f3f4f6',
          textAlign: 'right',
          overflow: 'hidden',
          textOverflow: 'ellipsis',
          whiteSpace: 'nowrap',
          maxWidth: '130px',
        }}
      >
        {value}
      </span>
    </div>
  )
}

interface Props {
  label: string
  apid: number
  variant: 'land' | 'uav' | 'cryobot'
}

export function VehicleInfoBox({ label, apid, variant }: Props) {
  const hk = useTelemetryStore((s) => s.hk)
  const selectNode = useUiStore((s) => s.selectNode)

  const snapshot = hk.find((s) => s.apid === apid)
  const latest = snapshot?.frames[snapshot.frames.length - 1]
  const decoded = latest ? decodeHkFrame(apid, latest.data) : {}
  const color = VARIANT_COLOR[variant] ?? '#00ff88'
  const icon = VARIANT_ICON[variant] ?? '●'
  const role = VARIANT_ROLE[variant] ?? ''
  const isActive = latest != null

  const timeStr = latest
    ? new Date(latest.timestamp_utc).toLocaleTimeString('en-GB', { hour12: false })
    : '—'

  return (
    <Html
      position={[0, 2.4, 0]}
      center
      distanceFactor={12}
      zIndexRange={[1000, 0]}
    >
      <div
        style={{
          background: 'rgba(2,4,8,0.91)',
          border: `1px solid ${color}44`,
          borderRadius: '12px',
          padding: '13px 15px',
          minWidth: '200px',
          maxWidth: '240px',
          backdropFilter: 'blur(18px)',
          boxShadow: `0 0 32px ${color}18, 0 0 0 1px ${color}11, 0 8px 24px rgba(0,0,0,0.85)`,
          fontFamily: '"JetBrains Mono", ui-monospace, monospace',
          fontSize: '11px',
          color: '#e5e7eb',
        }}
        onClick={(e) => e.stopPropagation()}
      >
        {/* Header */}
        <div
          style={{
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'flex-start',
            marginBottom: '10px',
          }}
        >
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <span style={{ fontSize: '18px', lineHeight: 1 }}>{icon}</span>
            <div>
              <div style={{ color, fontWeight: 700, fontSize: '12px', letterSpacing: '0.4px' }}>{label}</div>
              <div style={{ color: '#4b5563', fontSize: '9px', letterSpacing: '1.5px', marginTop: '1px' }}>{role}</div>
            </div>
          </div>
          <button
            style={{
              background: 'none',
              border: 'none',
              color: '#6b7280',
              cursor: 'pointer',
              fontSize: '17px',
              lineHeight: 1,
              padding: '0 2px',
              marginTop: '-2px',
            }}
            onClick={() => selectNode(null)}
          >
            ×
          </button>
        </div>

        {/* Identity */}
        <div
          style={{
            borderTop: `1px solid ${color}20`,
            borderBottom: `1px solid ${color}20`,
            padding: '7px 0',
            marginBottom: '8px',
            display: 'flex',
            flexDirection: 'column',
            gap: '2px',
          }}
        >
          <Row label="APID" value={`0x${apid.toString(16).toUpperCase().padStart(3, '0')}`} valueColor="#22d3ee" />
          <Row label="Last HK" value={timeStr} />
          <Row label="Frames" value={snapshot ? String(snapshot.frames.length) : '0'} />
        </div>

        {/* Live telemetry */}
        {Object.keys(decoded).length === 0 ? (
          <div style={{ color: '#374151', textAlign: 'center', padding: '6px 0', fontSize: '10px' }}>
            No telemetry
          </div>
        ) : (
          <div style={{ display: 'flex', flexDirection: 'column', gap: '3px' }}>
            {Object.entries(decoded).map(([k, v]) => (
              <Row key={k} label={k} value={String(v)} />
            ))}
          </div>
        )}

        {/* Status footer */}
        <div
          style={{
            marginTop: '10px',
            paddingTop: '7px',
            borderTop: `1px solid ${color}20`,
            display: 'flex',
            alignItems: 'center',
            gap: '7px',
          }}
        >
          <div
            style={{
              width: '7px',
              height: '7px',
              borderRadius: '50%',
              background: isActive ? color : '#ef4444',
              boxShadow: isActive ? `0 0 10px ${color}, 0 0 20px ${color}88` : 'none',
              flexShrink: 0,
            }}
          />
          <span style={{ color: isActive ? color : '#ef4444', fontSize: '10px', letterSpacing: '1.5px' }}>
            {isActive ? 'ACTIVE' : 'NO SIGNAL'}
          </span>
          {isActive && (
            <span style={{ color: '#374151', fontSize: '10px', marginLeft: 'auto' }}>
              MARS SOL
            </span>
          )}
        </div>
      </div>
    </Html>
  )
}
