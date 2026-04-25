import { useEffect, useRef } from 'react'
import { useTelemetryStore } from '../store/telemetryStore'
import type { WsSnapshot } from '../types/telemetry'

const WS_URL = '/ws'
const MIN_DELAY = 1000
const MAX_DELAY = 30000
const STABLE_THRESHOLD = 10000

export function useWebSocket() {
  const socketRef = useRef<WebSocket | null>(null)
  const reconnectDelayRef = useRef(MIN_DELAY)
  const reconnectTimerRef = useRef<ReturnType<typeof setTimeout> | null>(null)
  const connectTimeRef = useRef<number>(0)
  const applySnapshot = useTelemetryStore((s) => s.applySnapshot)
  const setConnected = useTelemetryStore((s) => s.setConnected)
  const isMockMode = useTelemetryStore((s) => s.isMockMode)

  useEffect(() => {
    if (isMockMode) {
      if (socketRef.current) {
        socketRef.current.close()
        socketRef.current = null
      }
      setConnected(false)
      return
    }

    function connect() {
      const ws = new WebSocket(WS_URL)
      socketRef.current = ws
      connectTimeRef.current = Date.now()

      ws.onopen = () => {
        setConnected(true)
        reconnectDelayRef.current = MIN_DELAY
      }

      ws.onmessage = (ev) => {
        try {
          const snap = JSON.parse(ev.data as string) as WsSnapshot
          if (!Array.isArray(snap.hk) || !Array.isArray(snap.links_health)) return
          applySnapshot(snap)
          if (Date.now() - connectTimeRef.current > STABLE_THRESHOLD) {
            reconnectDelayRef.current = MIN_DELAY
          }
        } catch {
          // malformed frame — keep connection alive
        }
      }

      ws.onclose = () => {
        setConnected(false)
        scheduleReconnect()
      }

      ws.onerror = () => {
        ws.close()
      }
    }

    function scheduleReconnect() {
      if (reconnectTimerRef.current) clearTimeout(reconnectTimerRef.current)
      reconnectTimerRef.current = setTimeout(() => {
        reconnectDelayRef.current = Math.min(reconnectDelayRef.current * 2, MAX_DELAY)
        connect()
      }, reconnectDelayRef.current)
    }

    connect()

    return () => {
      if (reconnectTimerRef.current) clearTimeout(reconnectTimerRef.current)
      if (socketRef.current) {
        socketRef.current.onclose = null
        socketRef.current.close()
        socketRef.current = null
      }
    }
  }, [isMockMode, applySnapshot, setConnected])
}
