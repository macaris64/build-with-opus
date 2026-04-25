import type { WsSnapshot, HkSnapshot, EventEntry } from '../types/telemetry'

let tick = 0
let cfdpProgress = 0
let suspectFireTick = 0

function f32LE(v: number): number[] {
  const buf = new ArrayBuffer(4)
  new DataView(buf).setFloat32(0, v, true)
  return Array.from(new Uint8Array(buf))
}

function u32LE(v: number): number[] {
  const buf = new ArrayBuffer(4)
  new DataView(buf).setUint32(0, v, true)
  return Array.from(new Uint8Array(buf))
}

function u32BE(v: number): number[] {
  const buf = new ArrayBuffer(4)
  new DataView(buf).setUint32(0, v, false)
  return Array.from(new Uint8Array(buf))
}

function makeHk(asset: string, apid: number, data: number[]): HkSnapshot {
  return {
    asset,
    apid,
    frames: [{ timestamp_utc: new Date().toISOString(), data }],
  }
}

function quatData(): number[] {
  const theta = tick * 0.02
  const w = Math.cos(theta / 2)
  const z = Math.sin(theta / 2)
  return [
    ...f32LE(w), ...f32LE(0), ...f32LE(0), ...f32LE(z),
    ...f32LE(Math.sin(theta) * 0.01),
    ...f32LE(Math.cos(theta) * 0.005),
    ...f32LE(0.001),
    0, 0, 0, 0,
  ]
}

function ros2BridgeData(): number[] {
  return [
    ...u32LE(tick * 3),
    ...u32LE(2),
    ...u32LE(tick),
    ...u32LE(tick),
    ...u32LE(tick * 3 + 2),
    ...u32LE(0),
  ]
}

function prx1Data(): number[] {
  const active = 1
  const strength = Math.round(180 + Math.sin(tick * 0.1) * 60)
  return [active, strength, 0, 0, 0, 0, 0, 0, 0, 0]
}

function fleetData(): number[] {
  const mask = 0x07
  const landAge = Math.round(800 + Math.sin(tick * 0.15) * 100)
  const uavAge = Math.round(900 + Math.sin(tick * 0.12) * 150)
  const cryoAge = Math.round(950 + Math.sin(tick * 0.08) * 200)
  return [mask, ...u32BE(landAge), ...u32BE(uavAge), ...u32BE(cryoAge)]
}

function powerData(): number[] {
  const voltage = Math.round(28000 + Math.sin(tick * 0.05) * 1400)
  return [
    ...[(voltage & 0xff), ((voltage >> 8) & 0xff)],
    0x5a, 0x00,
    0x3e, 0x08,
    0x00, 0x00,
  ]
}

function roverLandData(): number[] {
  const x = tick * 0.05
  const y = Math.sin(tick * 0.03) * 10
  const heading = (tick * 2) % 360
  return [...f32LE(x), ...f32LE(y), ...f32LE(heading), 0x14, 0x00]
}

function roverUavData(): number[] {
  const alt = 50 + Math.sin(tick * 0.05) * 10
  const x = Math.cos(tick * 0.03) * 30
  const y = Math.sin(tick * 0.03) * 30
  const batt = 85 - tick * 0.01
  return [...f32LE(alt), ...f32LE(x), ...f32LE(y), ...f32LE(Math.max(0, batt))]
}

function roverCryoData(): number[] {
  const depth = tick * 0.002
  const rpm = 450 + Math.sin(tick * 0.1) * 50
  const tempRaw = Math.round((-180 + Math.sin(tick * 0.02) * 5) * 10)
  const t16 = tempRaw < 0 ? tempRaw + 65536 : tempRaw
  return [...f32LE(depth), ...f32LE(rpm), t16 & 0xff, (t16 >> 8) & 0xff]
}

const EVENT_TEMPLATES: Array<[number, string, number]> = [
  [0x101, 'CDH: Scheduler cycle complete. Mode=NOMINAL', 1],
  [0x110, 'ADCS: Attitude estimate converged. Max dev=0.003 deg', 1],
  [0x120, 'COMM: VC0 rate adjusted to 100 kbps', 2],
  [0x128, 'ROS2_BRIDGE: PacketsRouted milestone 1000', 1],
  [0x130, 'POWER: Battery SOC 78%. Solar charging nominal', 1],
  [0x140, 'PAYLOAD: Science frame captured. Frame=1024', 1],
  [0x300, 'ROVER_LAND: Waypoint reached. Next target in 30s', 1],
  [0x3c0, 'UAV: Altitude hold engaged at 52m', 2],
  [0x400, 'CRYOBOT: Depth milestone 1.5m reached', 1],
  [0x110, 'ADCS: WARNING - angular rate above threshold (0.015 rad/s)', 3],
]

export function generateSnapshot(): WsSnapshot {
  tick++
  cfdpProgress = (cfdpProgress + 2) % 102

  const isAos = Math.floor(tick / 30) % 2 === 0
  const timeSuspect = tick - suspectFireTick >= 60 && tick % 60 === 0
  if (timeSuspect) suspectFireTick = tick

  const eventIdx = tick % EVENT_TEMPLATES.length
  const [evApid, evMsg, evSev] = EVENT_TEMPLATES[eventIdx]!
  const newEvent: EventEntry = {
    timestamp_utc: new Date().toISOString(),
    apid: evApid,
    severity: evSev,
    message: evMsg,
  }

  const hk: HkSnapshot[] = [
    makeHk('orbiter_cdh',    0x101, [0x01, 0x00, tick & 0xff, 0x00, 0x00, 0x00, ...u32LE(tick)]),
    makeHk('orbiter_adcs',   0x110, quatData()),
    makeHk('orbiter_comm',   0x120, [0x01, 0x00, 0x64, 0x00, tick & 0xff, 0x00, 0x00, 0x00]),
    makeHk('ros2_bridge',    0x128, ros2BridgeData()),
    makeHk('prx1_link',      0x129, prx1Data()),
    makeHk('orbiter_power',  0x130, powerData()),
    makeHk('orbiter_payload',0x140, [0x01, 0x00, 0xe8, 0x03, tick & 0xff, (tick >> 8) & 0xff]),
    makeHk('fleet_monitor',  0x160, fleetData()),
    makeHk('rover_land',     0x300, roverLandData()),
    makeHk('rover_uav',      0x3c0, roverUavData()),
    makeHk('rover_cryobot',  0x400, roverCryoData()),
  ]

  const linkName = (s: string) => ({ link: s, apid: s === 'cfs-ros' ? 0x128 : s === 'ros-ground' ? 0x129 : 0x160, last_hk_utc: new Date().toISOString() })

  return {
    hk,
    events: tick === 1 ? [] : [newEvent],
    cfdp: cfdpProgress < 100
      ? [{ transaction_id: 1, source_entity: 1, dest_entity: 0, progress_pct: cfdpProgress, complete: false }]
      : [{ transaction_id: 1, source_entity: 1, dest_entity: 0, progress_pct: 100, complete: true }],
    mfile: [{ transaction_id: 2, total_chunks: 100, received_chunks: Math.min(tick, 100), gaps: tick < 100 ? [[Math.min(tick, 99), 99]] : [] }],
    link: { state: isAos ? 'Aos' : 'Degraded', last_frame_utc: new Date().toISOString() },
    cop1: {
      fop1_state: 'Active',
      window_occupancy: Math.floor(Math.abs(Math.sin(tick * 0.2)) * 14),
      retransmit_count: tick % 30 === 0 ? 1 : 0,
    },
    time_auth: {
      tai_offset_s: 37,
      drift_budget_us_per_day: 83.3,
      sync_packet_age_ms: (tick % 60) * 1000,
      time_suspect_seen: timeSuspect,
    },
    links_health: ['cfs-ros', 'ros-ground', 'fleet-dds'].map(linkName),
  }
}
