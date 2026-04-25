import clsx from 'clsx'
import { useTelemetryStore } from '../store/telemetryStore'

export function CfdpProgress() {
  const cfdp = useTelemetryStore((s) => s.cfdp)
  const mfile = useTelemetryStore((s) => s.mfile)

  if (cfdp.length === 0 && mfile.length === 0) {
    return (
      <div className="text-xs text-gray-600 text-center py-2">No active file transfers</div>
    )
  }

  return (
    <div className="flex flex-col gap-2">
      {cfdp.map((tx) => (
        <div key={tx.transaction_id} className="bg-space-900 border border-gray-700/50 rounded-lg p-3">
          <div className="flex justify-between text-xs mb-2">
            <span className="text-gray-400 font-mono">CFDP #{tx.transaction_id}</span>
            <span className="text-gray-500">
              {tx.source_entity} → {tx.dest_entity}
            </span>
            {tx.complete && <span className="text-green-400 font-bold">✓ DONE</span>}
          </div>
          <div className="h-2 bg-gray-800 rounded-full overflow-hidden">
            <div
              className={clsx(
                'h-full rounded-full transition-all duration-500',
                tx.complete ? 'bg-green-500' : 'bg-cyan-600',
              )}
              style={{ width: `${tx.progress_pct}%` }}
            />
          </div>
          <div className="text-right text-xs text-gray-500 mt-1">{tx.progress_pct.toFixed(1)}%</div>
        </div>
      ))}

      {mfile.map((tx) => {
        const pct = tx.total_chunks > 0 ? (tx.received_chunks / tx.total_chunks) * 100 : 0
        return (
          <div key={tx.transaction_id} className="bg-space-900 border border-gray-700/50 rounded-lg p-3">
            <div className="flex justify-between text-xs mb-2">
              <span className="text-gray-400 font-mono">M-File #{tx.transaction_id}</span>
              <span className="text-gray-300">{tx.received_chunks}/{tx.total_chunks} chunks</span>
            </div>
            <div className="h-2 bg-gray-800 rounded-full overflow-hidden">
              <div
                className="h-full rounded-full bg-purple-600 transition-all duration-500"
                style={{ width: `${pct}%` }}
              />
            </div>
            {tx.gaps.length > 0 && (
              <div className="text-xs text-amber-500 mt-1">
                Gaps: {tx.gaps.map(([a, b]) => `${a}-${b}`).join(', ')}
              </div>
            )}
          </div>
        )
      })}
    </div>
  )
}
