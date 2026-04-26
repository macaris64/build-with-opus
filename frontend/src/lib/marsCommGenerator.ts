import type { MarsVehicle, CommEvent } from '../types/mars'

let eventCounter = 0

function rnd(min: number, max: number): number {
  return Math.random() * (max - min) + min
}

function rndInt(min: number, max: number): number {
  return Math.floor(rnd(min, max + 1))
}

function droneFields(v: MarsVehicle): Record<string, string> {
  const alt = rnd(v.basePosition[1] - 0.5, v.basePosition[1] + 0.5)
  return {
    alt: `${alt.toFixed(1)} m`,
    bat: `${rndInt(65, 99)} %`,
    hdg: `${rndInt(0, 359)}°`,
    rssi: `${rndInt(-85, -45)} dBm`,
  }
}

function roverFields(): Record<string, string> {
  return {
    'pos-x': `${rnd(-15, 15).toFixed(1)} m`,
    'pos-z': `${rnd(-15, 15).toFixed(1)} m`,
    bat: `${rndInt(60, 100)} %`,
    hdg: `${rndInt(0, 359)}°`,
    temp: `${rndInt(-20, 45)} °C`,
  }
}

function cryobotFields(): Record<string, string> {
  return {
    depth: `${rnd(0.1, 8.5).toFixed(2)} m`,
    rpm: `${rndInt(120, 800)}`,
    temp: `${-rndInt(10, 80)} °C`,
    bat: `${rndInt(70, 98)} %`,
  }
}

function apidName(type: MarsVehicle['type']): string {
  switch (type) {
    case 'drone': return 'UAV HK'
    case 'land': return 'LAND HK'
    case 'cryobot': return 'CRYO HK'
  }
}

export function buildCommEvent(from: MarsVehicle, to: MarsVehicle): CommEvent {
  const id = `evt-${++eventCounter}`
  const now = Date.now()
  let fields: Record<string, string>
  switch (from.type) {
    case 'drone': fields = droneFields(from); break
    case 'land': fields = roverFields(); break
    case 'cryobot': fields = cryobotFields(); break
  }
  return {
    id,
    fromId: from.id,
    toId: to.id,
    apid: from.apid,
    apidName: apidName(from.type),
    decodedFields: fields,
    timestamp: now,
    expiresAt: now + 2200,
  }
}

export function pickRandomPair(vehicles: MarsVehicle[]): [MarsVehicle, MarsVehicle] | null {
  if (vehicles.length < 2) return null
  const from = vehicles[rndInt(0, vehicles.length - 1)]
  const candidates = vehicles.filter((v) => v.id !== from.id)
  const to = candidates[rndInt(0, candidates.length - 1)]
  return [from, to]
}
