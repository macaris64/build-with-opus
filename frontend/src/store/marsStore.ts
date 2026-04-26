import { create } from 'zustand'
import type { MarsVehicle, CommEvent, VehicleType } from '../types/mars'

const MAX_LOG = 50

const INITIAL_VEHICLES: MarsVehicle[] = [
  { id: 'drone-1', type: 'drone', label: 'DRONE-1', apid: 0x3c0, instanceId: 1, basePosition: [0, 5, 0], seqCount: 0 },
  { id: 'drone-2', type: 'drone', label: 'DRONE-2', apid: 0x3c0, instanceId: 2, basePosition: [-6, 6, 4], seqCount: 0 },
  { id: 'drone-3', type: 'drone', label: 'DRONE-3', apid: 0x3c0, instanceId: 3, basePosition: [8, 4, -3], seqCount: 0 },
  { id: 'drone-4', type: 'drone', label: 'DRONE-4', apid: 0x3c0, instanceId: 4, basePosition: [-3, 7, -7], seqCount: 0 },
  { id: 'drone-5', type: 'drone', label: 'DRONE-5', apid: 0x3c0, instanceId: 5, basePosition: [5, 5, 7], seqCount: 0 },
  { id: 'rover-1', type: 'land', label: 'ROVER-1', apid: 0x300, instanceId: 1, basePosition: [6, 0, 2], seqCount: 0 },
  { id: 'rover-2', type: 'land', label: 'ROVER-2', apid: 0x300, instanceId: 2, basePosition: [-8, 0, 5], seqCount: 0 },
  { id: 'rover-3', type: 'land', label: 'ROVER-3', apid: 0x300, instanceId: 3, basePosition: [3, 0, -6], seqCount: 0 },
  { id: 'cryobot-1', type: 'cryobot', label: 'CRYOBOT-1', apid: 0x400, instanceId: 1, basePosition: [-4, 0, -3], seqCount: 0 },
]

export const VEHICLE_COLOR: Record<VehicleType, string> = {
  drone: '#38bdf8',
  land: '#f97316',
  cryobot: '#a78bfa',
}

export const APID_TO_VEHICLE_ID: Record<number, string> = {
  0x3c0: 'drone-1',
  0x3c1: 'drone-2',
  0x3c2: 'drone-3',
  0x3c3: 'drone-4',
  0x3c4: 'drone-5',
  0x300: 'rover-1',
  0x301: 'rover-2',
  0x302: 'rover-3',
  0x400: 'cryobot-1',
}

interface MarsStore {
  vehicles: MarsVehicle[]
  activeComms: CommEvent[]
  packetLog: CommEvent[]
  selectedVehicleId: string | null
  addComm: (event: CommEvent) => void
  addToLog: (event: CommEvent) => void
  expireComms: () => void
  updateVehicle: (id: string, patch: Partial<MarsVehicle>) => void
  selectVehicle: (id: string | null) => void
}

export const useMarsStore = create<MarsStore>((set) => ({
  vehicles: INITIAL_VEHICLES,
  activeComms: [],
  packetLog: [],
  selectedVehicleId: null,

  addComm: (event) =>
    set((s) => ({ activeComms: [...s.activeComms, event] })),

  addToLog: (event) =>
    set((s) => ({
      packetLog: [event, ...s.packetLog].slice(0, MAX_LOG),
    })),

  expireComms: () =>
    set((s) => ({
      activeComms: s.activeComms.filter((e) => Date.now() < e.expiresAt),
    })),

  updateVehicle: (id, patch) =>
    set((s) => ({
      vehicles: s.vehicles.map((v) => (v.id === id ? { ...v, ...patch } : v)),
    })),

  selectVehicle: (id) => set({ selectedVehicleId: id }),
}))
