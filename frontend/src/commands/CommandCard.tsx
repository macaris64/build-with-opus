import { useState } from 'react'
import clsx from 'clsx'
import type { CommandDef } from '../types/commands'
import { useUiStore, getCommandState } from '../store/uiStore'
import { useTelemetryStore } from '../store/telemetryStore'

interface Props {
  def: CommandDef
}

function computeValidUntil(taiOffsetS: number): number {
  return Math.floor(Date.now() / 1000) + taiOffsetS + 120
}

async function submitTc(apid: number, funcCode: number, validUntil: number): Promise<boolean> {
  const res = await fetch('/api/tc', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ valid_until_tai_coarse: validUntil, apid, func_code: funcCode }),
  })
  if (!res.ok) return false
  const json = (await res.json()) as { accepted: boolean }
  return json.accepted
}

export function CommandCard({ def }: Props) {
  const [showConfirm, setShowConfirm] = useState(false)
  const commandStates = useUiStore((s) => s.commandStates)
  const setCommandState = useUiStore((s) => s.setCommandState)
  const isMockMode = useTelemetryStore((s) => s.isMockMode)
  const taiOffset = useTelemetryStore((s) => s.time_auth.tai_offset_s)

  const cmdState = getCommandState(commandStates, def.id)

  async function doSend() {
    setCommandState(def.id, { status: 'pending', lastSentAt: null, errorMsg: null })
    try {
      if (isMockMode) {
        await new Promise((r) => setTimeout(r, 200))
        setCommandState(def.id, { status: 'accepted', lastSentAt: new Date().toISOString(), errorMsg: null })
        return
      }
      const validUntil = computeValidUntil(taiOffset)
      const ok = await submitTc(def.apid, def.funcCode, validUntil)
      setCommandState(def.id, {
        status: ok ? 'accepted' : 'rejected',
        lastSentAt: new Date().toISOString(),
        errorMsg: ok ? null : 'Validity window check failed or server error',
      })
    } catch (e) {
      setCommandState(def.id, {
        status: 'rejected',
        lastSentAt: new Date().toISOString(),
        errorMsg: String(e),
      })
    }
  }

  function onSend() {
    if (def.isSafetyInterlocked) {
      setShowConfirm(true)
    } else {
      void doSend()
    }
  }

  function onConfirm() {
    setShowConfirm(false)
    void doSend()
  }

  const statusColor = {
    idle: 'text-gray-500',
    pending: 'text-amber-400 animate-pulse',
    accepted: 'text-green-400',
    rejected: 'text-red-400',
  }[cmdState.status]

  const statusLabel = {
    idle: '—',
    pending: 'SENDING…',
    accepted: '✓ ACCEPTED',
    rejected: '✗ REJECTED',
  }[cmdState.status]

  return (
    <div className={clsx(
      'border rounded-lg p-3 bg-space-900 flex flex-col gap-2',
      def.isSafetyInterlocked ? 'border-amber-700/50' : 'border-gray-700/50',
    )}>
      {showConfirm && (
        <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/70">
          <div className="bg-gray-900 border border-amber-500 rounded-xl p-6 max-w-sm w-full mx-4">
            <p className="text-amber-400 font-bold mb-2">⚠ SAFETY-INTERLOCKED COMMAND</p>
            <p className="text-gray-200 text-sm mb-1">{def.name}</p>
            <p className="text-gray-400 text-xs mb-1">
              APID: <span className="text-cyan-400">0x{def.apid.toString(16).toUpperCase()}</span>
              {' '}FC: <span className="text-cyan-400">0x{def.funcCode.toString(16).toUpperCase()}</span>
            </p>
            <p className="text-gray-400 text-xs mb-4">{def.description}</p>
            <div className="flex gap-3">
              <button
                onClick={() => setShowConfirm(false)}
                className="flex-1 py-2 rounded bg-gray-700 hover:bg-gray-600 text-gray-200 text-sm transition-colors"
              >
                Cancel
              </button>
              <button
                onClick={onConfirm}
                className="flex-1 py-2 rounded bg-amber-600 hover:bg-amber-500 text-white text-sm font-bold transition-colors"
              >
                CONFIRM SEND
              </button>
            </div>
          </div>
        </div>
      )}

      <div className="flex items-start justify-between gap-2">
        <div className="flex-1 min-w-0">
          <div className="flex items-center gap-2">
            <span className="text-gray-100 text-sm font-semibold truncate">{def.name}</span>
            {def.isSafetyInterlocked && (
              <span className="text-xs text-amber-400 border border-amber-600 px-1 rounded shrink-0">INTLKD</span>
            )}
          </div>
          <div className="text-xs text-gray-500 mt-0.5">
            <span className="text-cyan-500">0x{def.apid.toString(16).toUpperCase()}</span>
            <span className="mx-1">·</span>
            <span className="text-purple-400">FC 0x{def.funcCode.toString(16).toUpperCase()}</span>
            <span className="mx-1">·</span>
            <span>{def.target}</span>
          </div>
        </div>
        <button
          onClick={onSend}
          disabled={cmdState.status === 'pending'}
          className={clsx(
            'shrink-0 px-3 py-1.5 rounded text-xs font-bold transition-colors',
            cmdState.status === 'pending'
              ? 'bg-gray-700 text-gray-500 cursor-not-allowed'
              : def.isSafetyInterlocked
              ? 'bg-amber-700 hover:bg-amber-600 text-white'
              : 'bg-cyan-700 hover:bg-cyan-600 text-white',
          )}
        >
          SEND
        </button>
      </div>

      <p className="text-xs text-gray-500 leading-relaxed">{def.description}</p>

      <div className="flex items-center justify-between text-xs pt-1 border-t border-gray-800">
        <span className={clsx('font-mono', statusColor)}>{statusLabel}</span>
        {cmdState.lastSentAt && (
          <span className="text-gray-600 text-xs">
            {new Date(cmdState.lastSentAt).toLocaleTimeString()}
          </span>
        )}
      </div>
    </div>
  )
}
