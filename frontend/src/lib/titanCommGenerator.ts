import type { TitanVehicle, TitanCommEvent } from '../types/titan'

let eventCounter = 0

function rnd(min: number, max: number): number {
  return Math.random() * (max - min) + min
}

function rndInt(min: number, max: number): number {
  return Math.floor(rnd(min, max + 1))
}

function uavFields(v: TitanVehicle): Record<string, string> {
  const alt = rnd(v.basePosition[1] - 0.5, v.basePosition[1] + 0.5)
  return {
    alt: `${alt.toFixed(1)} m`,
    bat: `${rndInt(60, 95)} %`,
    hdg: `${rndInt(0, 359)}°`,
    rssi: `${rndInt(-88, -50)} dBm`,
  }
}

function roverFields(): Record<string, string> {
  return {
    'pos-x': `${rnd(-20, 20).toFixed(1)} m`,
    'pos-z': `${rnd(-20, 20).toFixed(1)} m`,
    bat: `${rndInt(55, 90)} %`,
    hdg: `${rndInt(0, 359)}°`,
    temp: `${-rndInt(150, 180)} °C`,
  }
}

function cryobotFields(): Record<string, string> {
  return {
    depth: `${rnd(0.1, 50).toFixed(2)} m`,
    rpm: `${rndInt(200, 600)}`,
    temp: `${-rndInt(178, 183)} °C`,
    bat: `${rndInt(70, 95)} %`,
  }
}

function apidName(type: TitanVehicle['type']): string {
  switch (type) {
    case 'uav':     return 'TITAN UAV HK'
    case 'rover':   return 'TITAN ROVER HK'
    case 'cryobot': return 'TITAN CRYO HK'
  }
}

export function buildTitanCommEvent(from: TitanVehicle, to: TitanVehicle): TitanCommEvent {
  const id = `tevt-${++eventCounter}`
  const now = Date.now()
  let fields: Record<string, string>
  switch (from.type) {
    case 'uav':     fields = uavFields(from); break
    case 'rover':   fields = roverFields(); break
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

export function pickRandomTitanPair(vehicles: TitanVehicle[]): [TitanVehicle, TitanVehicle] | null {
  if (vehicles.length < 2) return null
  const from = vehicles[rndInt(0, vehicles.length - 1)]
  const candidates = vehicles.filter((v) => v.id !== from.id)
  const to = candidates[rndInt(0, candidates.length - 1)]
  if (!from || !to) return null
  return [from, to]
}
