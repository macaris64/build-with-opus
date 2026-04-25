import { useEffect, useRef, useState } from 'react'
import clsx from 'clsx'
import type { HkSnapshot } from '../types/telemetry'
import { decodeHkFrame } from '../lib/hkDecoder'
import { getApidName } from '../lib/apidRegistry'

interface Props {
  snapshot: HkSnapshot
}

export function HkCard({ snapshot }: Props) {
  const [expanded, setExpanded] = useState(false)
  const [flash, setFlash] = useState(false)
  const prevTimestampRef = useRef<string | null>(null)
  const latest = snapshot.frames[snapshot.frames.length - 1]
  const decoded = latest ? decodeHkFrame(snapshot.apid, latest.data) : {}

  useEffect(() => {
    const ts = latest?.timestamp_utc ?? null
    if (ts && ts !== prevTimestampRef.current) {
      prevTimestampRef.current = ts
      setFlash(true)
      const t = setTimeout(() => setFlash(false), 500)
      return () => clearTimeout(t)
    }
    return undefined
  }, [latest?.timestamp_utc])

  if (!latest) return null

  const timeStr = new Date(latest.timestamp_utc).toLocaleTimeString('en-GB', {
    hour12: false,
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit',
  })

  return (
    <div
      className={clsx(
        'border rounded-lg transition-all duration-300 overflow-hidden',
        flash ? 'border-green-500/70 bg-green-500/5' : 'border-gray-700/40 bg-space-900',
      )}
    >
      <button
        className="w-full text-left px-3 py-2 flex items-center justify-between gap-2"
        onClick={() => setExpanded((v) => !v)}
      >
        <div className="flex items-center gap-2 min-w-0">
          <span className="text-cyan-400 font-mono text-xs shrink-0">
            0x{snapshot.apid.toString(16).toUpperCase().padStart(3, '0')}
          </span>
          <span className="text-gray-200 text-sm truncate">
            {getApidName(snapshot.apid)}
          </span>
        </div>
        <div className="flex items-center gap-2 shrink-0">
          <span className="text-gray-500 text-xs">{timeStr}</span>
          <span className="text-gray-600 text-xs">{expanded ? '▲' : '▼'}</span>
        </div>
      </button>

      {expanded && (
        <div className="px-3 pb-3 border-t border-gray-800">
          <table className="w-full text-xs mt-2">
            <tbody>
              {Object.entries(decoded).map(([k, v]) => (
                <tr key={k} className="border-b border-gray-800/50 last:border-0">
                  <td className="py-0.5 text-gray-500 pr-3 font-mono">{k}</td>
                  <td className="py-0.5 text-gray-100 font-mono text-right">{String(v)}</td>
                </tr>
              ))}
            </tbody>
          </table>
          <p className="text-gray-600 text-xs mt-2">
            {snapshot.frames.length} frame{snapshot.frames.length !== 1 ? 's' : ''} buffered
          </p>
        </div>
      )}
    </div>
  )
}
