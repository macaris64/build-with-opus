import { useEffect, useState } from 'react'
import { useTelemetryStore } from '../store/telemetryStore'

export function TimeDisplay() {
  const timeAuth = useTelemetryStore((s) => s.time_auth)
  const lastUpdateAt = useTelemetryStore((s) => s.lastUpdateAt)
  const [now, setNow] = useState(() => Date.now())

  useEffect(() => {
    const id = setInterval(() => setNow(Date.now()), 100)
    return () => clearInterval(id)
  }, [])

  const utcStr = new Date(now).toISOString().replace('T', ' ').slice(0, 23) + ' UTC'
  const syncAgeMs = lastUpdateAt > 0
    ? timeAuth.sync_packet_age_ms + (now - lastUpdateAt)
    : 0
  const syncAgeS = (syncAgeMs / 1000).toFixed(1)

  return (
    <div className="bg-space-900 border border-gray-700/50 rounded-lg p-3 flex flex-col gap-2">
      <div className="flex items-center justify-between">
        <p className="text-xs font-bold text-gray-500 uppercase tracking-widest">Time</p>
        {timeAuth.time_suspect_seen && (
          <span className="relative flex items-center gap-1.5">
            <span className="absolute inline-flex h-full w-full rounded-full bg-amber-400 opacity-75 animate-ping" />
            <span className="relative inline-flex text-xs text-amber-300 font-bold px-2 py-0.5 bg-amber-500/20 border border-amber-500/50 rounded">
              ⚠ TIME SUSPECT
            </span>
          </span>
        )}
      </div>
      <p className="font-mono text-green-400 text-sm">{utcStr}</p>
      <div className="grid grid-cols-2 gap-x-4 gap-y-1 text-xs">
        <span className="text-gray-500">TAI offset</span>
        <span className="text-gray-200 text-right">+{timeAuth.tai_offset_s} s</span>
        <span className="text-gray-500">Drift budget</span>
        <span className="text-gray-200 text-right">{timeAuth.drift_budget_us_per_day.toFixed(1)} µs/day</span>
        <span className="text-gray-500">Sync age</span>
        <span className={`text-right font-mono ${syncAgeMs > 30000 ? 'text-amber-400' : 'text-gray-200'}`}>
          {syncAgeS} s
        </span>
      </div>
    </div>
  )
}
