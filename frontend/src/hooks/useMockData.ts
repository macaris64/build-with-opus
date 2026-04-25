import { useEffect } from 'react'
import { useTelemetryStore } from '../store/telemetryStore'
import { generateSnapshot } from '../lib/mockGenerator'

const MOCK_INTERVAL = 1000

export function useMockData() {
  const isMockMode = useTelemetryStore((s) => s.isMockMode)
  const applySnapshot = useTelemetryStore((s) => s.applySnapshot)
  const setConnected = useTelemetryStore((s) => s.setConnected)

  useEffect(() => {
    if (!isMockMode) return
    setConnected(true)
    applySnapshot(generateSnapshot())
    const id = setInterval(() => applySnapshot(generateSnapshot()), MOCK_INTERVAL)
    return () => {
      clearInterval(id)
      setConnected(false)
    }
  }, [isMockMode, applySnapshot, setConnected])
}
