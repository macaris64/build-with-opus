import { useEffect } from 'react'
import { useTelemetryStore } from '../store/telemetryStore'
import type { CfsRosLinkDto, RosGroundLinkDto, FleetDdsLinkDto } from '../types/telemetry'

const POLL_INTERVAL = 5000

async function fetchJson<T>(url: string): Promise<T> {
  const res = await fetch(url)
  if (!res.ok) throw new Error(`HTTP ${res.status}`)
  return res.json() as Promise<T>
}

export function useRestPolling() {
  const isMockMode = useTelemetryStore((s) => s.isMockMode)
  const applyRestLinks = useTelemetryStore((s) => s.applyRestLinks)

  useEffect(() => {
    if (isMockMode) return

    async function poll() {
      try {
        const [cfsRos, rosGround, fleetDds] = await Promise.all([
          fetchJson<CfsRosLinkDto>('/api/link/cfs-ros'),
          fetchJson<RosGroundLinkDto>('/api/link/ros-ground'),
          fetchJson<FleetDdsLinkDto>('/api/link/fleet-dds'),
        ])
        applyRestLinks(cfsRos, rosGround, fleetDds)
      } catch {
        // network error — keep last known state
      }
    }

    void poll()
    const id = setInterval(() => void poll(), POLL_INTERVAL)
    return () => clearInterval(id)
  }, [isMockMode, applyRestLinks])
}
