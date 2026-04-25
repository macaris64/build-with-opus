import { create } from 'zustand'
import type {
  HkSnapshot,
  EventEntry,
  CfdpStatus,
  MFileStatus,
  LinkStateDto,
  Cop1Status,
  TimeAuthority,
  LinkHealthSnapshot,
  WsSnapshot,
  CfsRosLinkDto,
  RosGroundLinkDto,
  FleetDdsLinkDto,
} from '../types/telemetry'

interface TelemetryStore {
  hk: HkSnapshot[]
  events: EventEntry[]
  cfdp: CfdpStatus[]
  mfile: MFileStatus[]
  link: LinkStateDto
  cop1: Cop1Status
  time_auth: TimeAuthority
  links_health: LinkHealthSnapshot[]
  cfsRosLink: CfsRosLinkDto | null
  rosGroundLink: RosGroundLinkDto | null
  fleetDdsLink: FleetDdsLinkDto | null
  isConnected: boolean
  isMockMode: boolean
  lastUpdateAt: number
  applySnapshot: (snap: WsSnapshot) => void
  applyRestLinks: (
    cfsRos: CfsRosLinkDto,
    rosGround: RosGroundLinkDto,
    fleetDds: FleetDdsLinkDto,
  ) => void
  setConnected: (connected: boolean) => void
  setMockMode: (mock: boolean) => void
}

const defaultLink: LinkStateDto = { state: 'Los', last_frame_utc: null }
const defaultCop1: Cop1Status = { fop1_state: 'Initial', window_occupancy: 0, retransmit_count: 0 }
const defaultTimeAuth: TimeAuthority = {
  tai_offset_s: 37,
  drift_budget_us_per_day: 83.3,
  sync_packet_age_ms: 0,
  time_suspect_seen: false,
}

export const useTelemetryStore = create<TelemetryStore>((set) => ({
  hk: [],
  events: [],
  cfdp: [],
  mfile: [],
  link: defaultLink,
  cop1: defaultCop1,
  time_auth: defaultTimeAuth,
  links_health: [],
  cfsRosLink: null,
  rosGroundLink: null,
  fleetDdsLink: null,
  isConnected: false,
  isMockMode: false,
  lastUpdateAt: 0,

  applySnapshot: (snap) =>
    set((state) => ({
      hk: snap.hk ?? state.hk,
      events: snap.events ?? state.events,
      cfdp: snap.cfdp ?? state.cfdp,
      mfile: snap.mfile ?? state.mfile,
      link: snap.link ?? state.link,
      cop1: snap.cop1 ?? state.cop1,
      time_auth: snap.time_auth ?? state.time_auth,
      links_health: snap.links_health ?? state.links_health,
      lastUpdateAt: Date.now(),
    })),

  applyRestLinks: (cfsRos, rosGround, fleetDds) =>
    set({
      cfsRosLink: cfsRos,
      rosGroundLink: rosGround,
      fleetDdsLink: fleetDds,
    }),

  setConnected: (connected) => set({ isConnected: connected }),
  setMockMode: (mock) => set({ isMockMode: mock }),
}))
