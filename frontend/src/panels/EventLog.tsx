import { useEffect, useRef, useState } from 'react'
import clsx from 'clsx'
import { useTelemetryStore } from '../store/telemetryStore'

const SEV_LABELS: Record<number, string> = { 1: 'INFO', 2: 'WARN', 3: 'ERROR', 4: 'CRIT' }
const SEV_COLORS: Record<number, string> = {
  1: 'text-gray-400',
  2: 'text-amber-400',
  3: 'text-red-400',
  4: 'text-red-300 font-bold animate-pulse',
}

export function EventLog() {
  const events = useTelemetryStore((s) => s.events)
  const bottomRef = useRef<HTMLDivElement>(null)
  const [paused, setPaused] = useState(false)

  useEffect(() => {
    if (!paused) {
      bottomRef.current?.scrollIntoView({ behavior: 'smooth' })
    }
  }, [events.length, paused])

  return (
    <div className="flex flex-col gap-2 h-full">
      <div className="flex items-center justify-between">
        <span className="text-xs text-gray-500">{events.length} events</span>
        <button
          onClick={() => setPaused((v) => !v)}
          className={clsx(
            'text-xs px-2 py-0.5 rounded border transition-colors',
            paused
              ? 'border-amber-500/50 text-amber-400 bg-amber-500/10'
              : 'border-gray-700 text-gray-500 hover:text-gray-300',
          )}
        >
          {paused ? '▶ RESUME' : '⏸ PAUSE'}
        </button>
      </div>

      <div className="overflow-y-auto max-h-80 flex flex-col gap-0.5 pr-1">
        {events.map((ev, i) => {
          const t = new Date(ev.timestamp_utc)
          const ts = t.toLocaleTimeString('en-GB', { hour12: false }) + '.' + String(t.getMilliseconds()).padStart(3, '0')
          return (
            <div key={i} className="flex gap-2 text-xs font-mono leading-5">
              <span className="text-gray-600 shrink-0">{ts}</span>
              <span className={clsx('shrink-0 w-10', SEV_COLORS[ev.severity])}>
                {SEV_LABELS[ev.severity] ?? String(ev.severity)}
              </span>
              <span className="text-cyan-600 shrink-0">0x{ev.apid.toString(16).toUpperCase().padStart(3, '0')}</span>
              <span className="text-gray-300 break-words">{ev.message}</span>
            </div>
          )
        })}
        <div ref={bottomRef} />
      </div>
    </div>
  )
}
