import clsx from 'clsx'
import { useTelemetryStore } from '../store/telemetryStore'

const STATE_COLORS: Record<string, string> = {
  Active: 'text-green-400',
  Initializing: 'text-amber-400',
  Initial: 'text-gray-400',
  Suspended: 'text-red-400',
  'RETRANSMIT_WITHOUT_WAIT': 'text-red-400',
  'RETRANSMIT_WITH_WAIT': 'text-red-300',
}

const COP1_WINDOW = 15

export function Cop1Status() {
  const cop1 = useTelemetryStore((s) => s.cop1)

  const stateColor = STATE_COLORS[cop1.fop1_state] ?? 'text-gray-400'

  return (
    <div className="bg-space-900 border border-gray-700/50 rounded-lg p-3 flex flex-col gap-2">
      <p className="text-xs font-bold text-gray-500 uppercase tracking-widest">COP-1 / FOP-1</p>

      <div className="flex items-center justify-between">
        <span className="text-xs text-gray-400">State</span>
        <span className={clsx('text-xs font-bold font-mono', stateColor)}>{cop1.fop1_state}</span>
      </div>

      <div>
        <div className="flex justify-between text-xs text-gray-500 mb-1">
          <span>Window ({cop1.window_occupancy}/{COP1_WINDOW})</span>
          {cop1.retransmit_count > 0 && (
            <span className="text-red-400 font-bold">RTX: {cop1.retransmit_count}</span>
          )}
        </div>
        <div className="flex gap-0.5">
          {Array.from({ length: COP1_WINDOW }, (_, i) => (
            <div
              key={i}
              className={clsx(
                'flex-1 h-3 rounded-sm transition-colors duration-300',
                i < cop1.window_occupancy
                  ? cop1.retransmit_count > 0
                    ? 'bg-red-500'
                    : 'bg-green-500'
                  : 'bg-gray-700',
              )}
            />
          ))}
        </div>
      </div>
    </div>
  )
}
