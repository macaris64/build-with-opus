import { useEffect, useRef } from 'react'
import { useTelemetryStore } from '../store/telemetryStore'
import { useTitanStore, TITAN_APID_TO_VEHICLE_ID } from '../store/titanStore'
import { decodeHkFrame } from '../lib/hkDecoder'
import { getApidName } from '../lib/apidRegistry'
import type { TitanCommEvent, TitanVehicle } from '../types/titan'

const TITAN_APIDS = new Set(Object.keys(TITAN_APID_TO_VEHICLE_ID).map(Number))

let eventCounter = 0

function num(v: string | number | undefined): number | undefined {
  const n = parseFloat(String(v ?? ''))
  return isNaN(n) ? undefined : n
}

function decodedToVehiclePatch(apid: number, decoded: Record<string, string | number>): Partial<TitanVehicle> {
  if (apid >= 0x420 && apid <= 0x424) {
    return {
      altitude: num(decoded.altitude_m),
      battery: num(decoded.battery_pct),
      posX: num(decoded.x_m),
      posZ: num(decoded.y_m),
    }
  }
  if (apid >= 0x410 && apid <= 0x412) {
    return {
      posX: num(decoded.x_m),
      posZ: num(decoded.y_m),
      heading: num(decoded.heading_deg),
    }
  }
  if (apid >= 0x430 && apid <= 0x433) {
    return {
      drillDepth: num(decoded.depth_m),
      motorRpm: num(decoded.drill_rpm),
      temperature: num(decoded.borehole_temp_C),
    }
  }
  return {}
}

export function useTitanBackendSync() {
  const hk = useTelemetryStore((s) => s.hk)
  const addComm = useTitanStore((s) => s.addComm)
  const addToLog = useTitanStore((s) => s.addToLog)
  const expireComms = useTitanStore((s) => s.expireComms)
  const updateVehicle = useTitanStore((s) => s.updateVehicle)
  const vehicles = useTitanStore((s) => s.vehicles)

  const lastSeenTs = useRef(new Map<number, string>())
  const vehiclesRef = useRef(vehicles)
  vehiclesRef.current = vehicles

  useEffect(() => {
    const timer = setInterval(() => expireComms(), 500)
    return () => clearInterval(timer)
  }, [expireComms])

  useEffect(() => {
    for (const snap of hk) {
      if (!TITAN_APIDS.has(snap.apid)) continue

      const latestFrame = snap.frames[snap.frames.length - 1]
      if (!latestFrame) continue

      const prevTs = lastSeenTs.current.get(snap.apid)
      if (latestFrame.timestamp_utc === prevTs) continue
      lastSeenTs.current.set(snap.apid, latestFrame.timestamp_utc)

      const vehicleId = TITAN_APID_TO_VEHICLE_ID[snap.apid]
      if (!vehicleId) continue

      const decoded = decodeHkFrame(snap.apid, latestFrame.data)
      const patch = decodedToVehiclePatch(snap.apid, decoded)
      const currentVehicle = vehiclesRef.current.find((v) => v.id === vehicleId)
      updateVehicle(vehicleId, {
        ...patch,
        lastPacketAt: new Date(latestFrame.timestamp_utc).getTime(),
        seqCount: (currentVehicle?.seqCount ?? 0) + 1,
      })

      const others = vehiclesRef.current.filter((v) => v.id !== vehicleId)
      if (others.length === 0) continue
      const to = others[Math.floor(Math.random() * others.length)]

      const event: TitanCommEvent = {
        id: `tbe-${++eventCounter}`,
        fromId: vehicleId,
        toId: to.id,
        apid: snap.apid,
        apidName: getApidName(snap.apid),
        decodedFields: Object.fromEntries(
          Object.entries(decoded).map(([k, v]) => [k, String(v)])
        ),
        timestamp: new Date(latestFrame.timestamp_utc).getTime(),
        expiresAt: Date.now() + 2200,
      }
      addComm(event)
      addToLog(event)
    }
  }, [hk, addComm, addToLog, updateVehicle])
}
