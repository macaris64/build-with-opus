import { create } from 'zustand'
import type { TitanVehicle, TitanCommEvent, TitanVehicleType } from '../types/titan'

const MAX_LOG = 50

const INITIAL_VEHICLES: TitanVehicle[] = [
  // UAVs — high altitude in Titan's thick atmosphere
  { id: 'titan-uav-1',   type: 'uav',     label: 'UAV-T1',   apid: 0x420, instanceId: 1, basePosition: [0, 12, 0],    seqCount: 0 },
  { id: 'titan-uav-2',   type: 'uav',     label: 'UAV-T2',   apid: 0x421, instanceId: 1, basePosition: [-7, 14, 5],   seqCount: 0 },
  { id: 'titan-uav-3',   type: 'uav',     label: 'UAV-T3',   apid: 0x422, instanceId: 1, basePosition: [9, 11, -4],   seqCount: 0 },
  { id: 'titan-uav-4',   type: 'uav',     label: 'UAV-T4',   apid: 0x423, instanceId: 1, basePosition: [-3, 13, -8],  seqCount: 0 },
  { id: 'titan-uav-5',   type: 'uav',     label: 'UAV-T5',   apid: 0x424, instanceId: 1, basePosition: [6, 15, 6],    seqCount: 0 },
  // Land Rovers — in the mountains
  { id: 'titan-rover-1', type: 'rover',   label: 'ROVER-T1', apid: 0x410, instanceId: 1, basePosition: [12, 2, 8],    seqCount: 0 },
  { id: 'titan-rover-2', type: 'rover',   label: 'ROVER-T2', apid: 0x411, instanceId: 1, basePosition: [-11, 3, 6],   seqCount: 0 },
  { id: 'titan-rover-3', type: 'rover',   label: 'ROVER-T3', apid: 0x412, instanceId: 1, basePosition: [8, 2.5, -10], seqCount: 0 },
  // Cryobots — in the methane lake
  { id: 'titan-cryo-1',  type: 'cryobot', label: 'CRYO-T1',  apid: 0x430, instanceId: 1, basePosition: [-6, 0.1, -3], seqCount: 0 },
  { id: 'titan-cryo-2',  type: 'cryobot', label: 'CRYO-T2',  apid: 0x431, instanceId: 1, basePosition: [-8, 0.1, -6], seqCount: 0 },
  { id: 'titan-cryo-3',  type: 'cryobot', label: 'CRYO-T3',  apid: 0x432, instanceId: 1, basePosition: [-5, 0.1, -8], seqCount: 0 },
  { id: 'titan-cryo-4',  type: 'cryobot', label: 'CRYO-T4',  apid: 0x433, instanceId: 1, basePosition: [-9, 0.1, -4], seqCount: 0 },
]

export const TITAN_VEHICLE_COLOR: Record<TitanVehicleType, string> = {
  uav:     '#fbbf24',
  rover:   '#78350f',
  cryobot: '#0891b2',
}

export const TITAN_APID_TO_VEHICLE_ID: Record<number, string> = {
  0x420: 'titan-uav-1',
  0x421: 'titan-uav-2',
  0x422: 'titan-uav-3',
  0x423: 'titan-uav-4',
  0x424: 'titan-uav-5',
  0x410: 'titan-rover-1',
  0x411: 'titan-rover-2',
  0x412: 'titan-rover-3',
  0x430: 'titan-cryo-1',
  0x431: 'titan-cryo-2',
  0x432: 'titan-cryo-3',
  0x433: 'titan-cryo-4',
}

interface TitanStore {
  vehicles: TitanVehicle[]
  activeComms: TitanCommEvent[]
  packetLog: TitanCommEvent[]
  selectedVehicleId: string | null
  addComm: (event: TitanCommEvent) => void
  addToLog: (event: TitanCommEvent) => void
  expireComms: () => void
  updateVehicle: (id: string, patch: Partial<TitanVehicle>) => void
  selectVehicle: (id: string | null) => void
}

export const useTitanStore = create<TitanStore>((set) => ({
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
