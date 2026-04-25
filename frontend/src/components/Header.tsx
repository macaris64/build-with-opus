import { useEffect, useState } from 'react'
import clsx from 'clsx'
import { useTelemetryStore } from '../store/telemetryStore'

export function Header() {
  const isConnected = useTelemetryStore((s) => s.isConnected)
  const isMockMode = useTelemetryStore((s) => s.isMockMode)
  const setMockMode = useTelemetryStore((s) => s.setMockMode)
  const [utc, setUtc] = useState('')

  useEffect(() => {
    function tick() {
      setUtc(new Date().toISOString().replace('T', ' ').slice(0, 19) + ' UTC')
    }
    tick()
    const id = setInterval(tick, 1000)
    return () => clearInterval(id)
  }, [])

  return (
    <header className="h-14 bg-space-950 border-b border-gray-800 px-6 flex items-center justify-between shrink-0 z-10">
      <div className="flex items-center gap-4">
        {/* Connection dot */}
        <span
          className={clsx(
            'inline-block w-2.5 h-2.5 rounded-full transition-colors duration-500',
            isConnected ? 'bg-green-400 shadow-[0_0_8px_#00ff00]' : 'bg-red-500',
          )}
        />
        <span className="font-bold text-gray-100 tracking-wider text-sm">
          SAKURA-II
          <span className="text-gray-500 font-normal ml-1">Ground System</span>
        </span>
      </div>

      <span className="font-mono text-green-400 text-sm hidden md:block">{utc}</span>

      <div className="flex items-center gap-3">
        <button
          onClick={() => setMockMode(!isMockMode)}
          className={clsx(
            'px-4 py-1.5 rounded-full text-xs font-bold border transition-all duration-200',
            isMockMode
              ? 'bg-amber-500/20 border-amber-500/60 text-amber-300 hover:bg-amber-500/30'
              : 'bg-green-500/15 border-green-500/50 text-green-400 hover:bg-green-500/25',
          )}
        >
          {isMockMode ? '◉ DEMO' : '● LIVE'}
        </button>
      </div>
    </header>
  )
}
