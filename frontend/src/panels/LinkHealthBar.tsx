import { useTelemetryStore } from '../store/telemetryStore'
import { StatusBadge } from '../components/StatusBadge'

function freshnessAge(utc: string | null): { label: string; variant: 'green' | 'amber' | 'red' } {
  if (!utc) return { label: 'NO DATA', variant: 'red' }
  const ageMs = Date.now() - new Date(utc).getTime()
  if (ageMs < 5000) return { label: `${(ageMs / 1000).toFixed(1)}s ago`, variant: 'green' }
  if (ageMs < 15000) return { label: `${(ageMs / 1000).toFixed(0)}s ago`, variant: 'amber' }
  return { label: `${(ageMs / 1000).toFixed(0)}s ago`, variant: 'red' }
}

const LINK_LABELS: Record<string, string> = {
  'cfs-ros': 'cFS ↔ ROS',
  'ros-ground': 'ROS ↔ Ground',
  'fleet-dds': 'DDS Fleet',
}

export function LinkHealthBar() {
  const linksHealth = useTelemetryStore((s) => s.links_health)
  const link = useTelemetryStore((s) => s.link)
  const cfsRos = useTelemetryStore((s) => s.cfsRosLink)

  const aosVariant: 'green' | 'amber' | 'red' =
    link.state === 'Aos' ? 'green' : link.state === 'Degraded' ? 'amber' : 'red'

  return (
    <div className="bg-space-900 border border-gray-700/50 rounded-lg p-3 flex flex-col gap-2">
      <p className="text-xs font-bold text-gray-500 uppercase tracking-widest">Comm Links</p>

      {/* AOS link (cFS ↔ Ground) */}
      <div className="flex items-center justify-between">
        <div className="flex items-center gap-2">
          <span
            className={`w-2 h-2 rounded-full ${
              link.state === 'Aos' ? 'bg-green-400' : link.state === 'Degraded' ? 'bg-amber-400' : 'bg-red-500'
            }`}
          />
          <span className="text-xs text-gray-300">cFS ↔ Ground (AOS)</span>
        </div>
        <div className="flex items-center gap-2">
          <StatusBadge
            variant={aosVariant}
            label={link.state.toUpperCase()}
          />
          {link.last_frame_utc && (
            <span className="text-xs text-gray-500">{freshnessAge(link.last_frame_utc).label}</span>
          )}
        </div>
      </div>

      {/* APID-based links */}
      {linksHealth.map((lh) => {
        const { label, variant } = freshnessAge(lh.last_hk_utc)
        return (
          <div key={lh.link} className="flex items-center justify-between">
            <div className="flex items-center gap-2">
              <span className={`w-2 h-2 rounded-full bg-${variant === 'green' ? 'green' : variant === 'amber' ? 'amber' : 'red'}-400`} />
              <span className="text-xs text-gray-300">
                {LINK_LABELS[lh.link] ?? lh.link}
              </span>
              <span className="text-xs text-gray-600">
                APID 0x{lh.apid.toString(16).toUpperCase()}
              </span>
            </div>
            <StatusBadge variant={variant} label={label} />
          </div>
        )
      })}

      {/* cFS↔ROS detail when available */}
      {cfsRos && (
        <div className="border-t border-gray-800 pt-2 text-xs text-gray-500 flex gap-4">
          <span>Routed: <span className="text-cyan-400">{cfsRos.packets_routed}</span></span>
          <span>TC fwd: <span className="text-purple-400">{cfsRos.tc_forwarded}</span></span>
          <span>Uptime: <span className="text-gray-300">{cfsRos.uptime_s}s</span></span>
        </div>
      )}
    </div>
  )
}
