import { useTelemetryStore } from '../store/telemetryStore'
import { HkCard } from './HkCard'

export function HkPanel() {
  const hk = useTelemetryStore((s) => s.hk)
  const populated = hk.filter((s) => s.frames.length > 0)

  if (populated.length === 0) {
    return (
      <div className="text-center py-8 text-gray-600 text-sm">
        Waiting for telemetry…
      </div>
    )
  }

  return (
    <div className="flex flex-col gap-2">
      {populated
        .slice()
        .sort((a, b) => a.apid - b.apid)
        .map((snap) => (
          <HkCard key={snap.apid} snapshot={snap} />
        ))}
    </div>
  )
}
