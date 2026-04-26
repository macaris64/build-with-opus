import { useEffect, useRef } from 'react'
import { useTitanStore } from '../store/titanStore'
import { buildTitanCommEvent, pickRandomTitanPair } from '../lib/titanCommGenerator'

export function useTitanComm() {
  const vehicles = useTitanStore((s) => s.vehicles)
  const addComm = useTitanStore((s) => s.addComm)
  const addToLog = useTitanStore((s) => s.addToLog)
  const expireComms = useTitanStore((s) => s.expireComms)
  const updateVehicle = useTitanStore((s) => s.updateVehicle)

  const vehiclesRef = useRef(vehicles)
  vehiclesRef.current = vehicles

  useEffect(() => {
    const expireTimer = setInterval(() => expireComms(), 500)

    let commTimer: ReturnType<typeof setTimeout>

    function scheduleNext() {
      const delay = 1500 + Math.random() * 2500
      commTimer = setTimeout(() => {
        const pair = pickRandomTitanPair(vehiclesRef.current)
        if (pair) {
          const [from, to] = pair
          const event = buildTitanCommEvent(from, to)
          addComm(event)
          addToLog(event)
          const patch: Record<string, number> = {
            seqCount: (from.seqCount ?? 0) + 1,
            lastPacketAt: event.timestamp,
          }
          const fields = event.decodedFields
          if (from.type === 'uav') {
            const altStr = fields['alt']
            if (altStr) patch['altitude'] = parseFloat(altStr)
            const batStr = fields['bat']
            if (batStr) patch['battery'] = parseInt(batStr)
            const hdgStr = fields['hdg']
            if (hdgStr) patch['heading'] = parseInt(hdgStr)
          } else if (from.type === 'rover') {
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
