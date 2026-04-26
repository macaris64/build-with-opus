import { Html } from '@react-three/drei'
import { useTitanStore, TITAN_VEHICLE_COLOR } from '../store/titanStore'
import type { TitanVehicle } from '../types/titan'

const ICONS: Record<string, string> = { uav: '✈', rover: '🚙', cryobot: '🔩' }
const ROLES: Record<string, string> = { uav: 'TITAN UAV', rover: 'TITAN ROVER', cryobot: 'METHANE DRILLER' }

function Row({ label, value }: { label: string; value: string }) {
  return (
    <div style={{ display: 'flex', justifyContent: 'space-between', gap: '10px', lineHeight: '1.6' }}>
      <span style={{ color: '#6b7280', flexShrink: 0 }}>{label}</span>
      <span style={{ color: '#f3f4f6', textAlign: 'right', whiteSpace: 'nowrap' }}>{value}</span>
    </div>
  )
}

export function TitanVehicleInfoBox({ vehicle }: { vehicle: TitanVehicle }) {
  const selectVehicle = useTitanStore((s) => s.selectVehicle)
  const color = TITAN_VEHICLE_COLOR[vehicle.type]
  const icon = ICONS[vehicle.type] ?? '●'
  const role = ROLES[vehicle.type] ?? ''

  const timeStr = vehicle.lastPacketAt
    ? new Date(vehicle.lastPacketAt).toLocaleTimeString('en-GB', { hour12: false })
    : '—'

  const fields: [string, string][] = []
  if (vehicle.altitude  !== undefined) fields.push(['alt',   `${vehicle.altitude.toFixed(1)} m`])
  if (vehicle.battery   !== undefined) fields.push(['bat',   `${vehicle.battery.toFixed(0)} %`])
  if (vehicle.posX      !== undefined) fields.push(['pos-x', `${vehicle.posX.toFixed(1)} m`])
  if (vehicle.posZ      !== undefined) fields.push(['pos-z', `${vehicle.posZ.toFixed(1)} m`])
  if (vehicle.heading   !== undefined) fields.push(['hdg',   `${vehicle.heading.toFixed(0)}°`])
  if (vehicle.drillDepth !== undefined) fields.push(['depth', `${vehicle.drillDepth.toFixed(2)} m`])
  if (vehicle.motorRpm  !== undefined) fields.push(['rpm',   `${vehicle.motorRpm.toFixed(0)}`])
  if (vehicle.temperature !== undefined) fields.push(['temp', `${vehicle.temperature.toFixed(1)} °C`])

  return (
    <Html position={[0, 2.6, 0]} center distanceFactor={12} zIndexRange={[1000, 0]}>
      <div
        style={{
          background: 'rgba(2,4,8,0.92)',
          border: `1px solid ${color}44`,
          borderRadius: '12px',
          padding: '13px 15px',
          minWidth: '200px',
          maxWidth: '240px',
          backdropFilter: 'blur(18px)',
          boxShadow: `0 0 32px ${color}18, 0 8px 24px rgba(0,0,0,0.85)`,
          fontFamily: '"JetBrains Mono", ui-monospace, monospace',
          fontSize: '11px',
          color: '#e5e7eb',
        }}
        onClick={(e) => e.stopPropagation()}
      >
        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start', marginBottom: '10px' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <span style={{ fontSize: '18px', lineHeight: 1 }}>{icon}</span>
            <div>
              <div style={{ color, fontWeight: 700, fontSize: '12px', letterSpacing: '0.4px' }}>{vehicle.label}</div>
              <div style={{ color: '#4b5563', fontSize: '9px', letterSpacing: '1.5px', marginTop: '1px' }}>{role}</div>
            </div>
          </div>
          <button
            style={{ background: 'none', border: 'none', color: '#6b7280', cursor: 'pointer', fontSize: '17px', lineHeight: 1, padding: '0 2px', marginTop: '-2px' }}
            onClick={() => selectVehicle(null)}
          >
            ×
          </button>
        </div>

        <div style={{ borderTop: `1px solid ${color}20`, borderBottom: `1px solid ${color}20`, padding: '7px 0', marginBottom: '8px', display: 'flex', flexDirection: 'column', gap: '2px' }}>
          <Row label="APID" value={`0x${vehicle.apid.toString(16).toUpperCase().padStart(3, '0')} inst=${vehicle.instanceId}`} />
          <Row label="Last PKT" value={timeStr} />
          <Row label="Seq" value={String(vehicle.seqCount)} />
        </div>

        {fields.length === 0 ? (
          <div style={{ color: '#374151', textAlign: 'center', padding: '6px 0', fontSize: '10px' }}>Awaiting telemetry</div>
        ) : (
          <div style={{ display: 'flex', flexDirection: 'column', gap: '3px' }}>
            {fields.map(([k, v]) => <Row key={k} label={k} value={v} />)}
          </div>
        )}

        <div style={{ marginTop: '10px', paddingTop: '7px', borderTop: `1px solid ${color}20`, display: 'flex', alignItems: 'center', gap: '7px' }}>
          <div style={{ width: '7px', height: '7px', borderRadius: '50%', background: color, boxShadow: `0 0 10px ${color}`, flexShrink: 0 }} />
          <span style={{ color, fontSize: '10px', letterSpacing: '1.5px' }}>ACTIVE</span>
          <span style={{ color: '#374151', fontSize: '10px', marginLeft: 'auto' }}>TITAN SOL</span>
        </div>
      </div>
    </Html>
  )
}
