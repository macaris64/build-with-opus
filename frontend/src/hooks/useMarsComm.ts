import { useEffect, useRef } from 'react'
import { useMarsStore } from '../store/marsStore'
import { buildCommEvent, pickRandomPair } from '../lib/marsCommGenerator'

export function useMarsComm() {
  const vehicles = useMarsStore((s) => s.vehicles)
  const addComm = useMarsStore((s) => s.addComm)
  const addToLog = useMarsStore((s) => s.addToLog)
  const expireComms = useMarsStore((s) => s.expireComms)
  const updateVehicle = useMarsStore((s) => s.updateVehicle)

  const vehiclesRef = useRef(vehicles)
  vehiclesRef.current = vehicles

  useEffect(() => {
    const expireTimer = setInterval(() => expireComms(), 500)

    let commTimer: ReturnType<typeof setTimeout>

    function scheduleNext() {
      const delay = 1500 + Math.random() * 2500
      commTimer = setTimeout(() => {
        const pair = pickRandomPair(vehiclesRef.current)
        if (pair) {
          const [from, to] = pair
          const event = buildCommEvent(from, to)
          addComm(event)
          addToLog(event)
          // Update sender's telemetry state
          const patch: Record<string, number> = {
            seqCount: (from.seqCount ?? 0) + 1,
            lastPacketAt: event.timestamp,
          }
          const fields = event.decodedFields
          if (from.type === 'drone') {
            const altStr = fields['alt']
            if (altStr) patch['altitude'] = parseFloat(altStr)
            const batStr = fields['bat']
            if (batStr) patch['battery'] = parseInt(batStr)
            const hdgStr = fields['hdg']
            if (hdgStr) patch['heading'] = parseInt(hdgStr)
          } else if (from.type === 'land') {
            const batStr = fields['bat']
            if (batStr) patch['battery'] = parseInt(batStr)
            const hdgStr = fields['hdg']
            if (hdgStr) patch['heading'] = parseInt(hdgStr)
          } else if (from.type === 'cryobot') {
            const depthStr = fields['depth']
            if (depthStr) patch['drillDepth'] = parseFloat(depthStr)
            const rpmStr = fields['rpm']
            if (rpmStr) patch['motorRpm'] = parseInt(rpmStr)
            const tempStr = fields['temp']
            if (tempStr) patch['temperature'] = parseInt(tempStr)
            const batStr = fields['bat']
            if (batStr) patch['battery'] = parseInt(batStr)
          }
          updateVehicle(from.id, patch as Partial<typeof from>)
        }
        scheduleNext()
      }, delay)
    }

    scheduleNext()

    return () => {
      clearInterval(expireTimer)
      clearTimeout(commTimer)
    }
  }, [addComm, addToLog, expireComms, updateVehicle])
}
