export interface HkFrame {
  timestamp_utc: string
  data: number[]
}

export interface HkSnapshot {
  asset: string
  apid: number
  frames: HkFrame[]
}

export interface EventEntry {
  timestamp_utc: string
  apid: number
  severity: number
  message: string
}

export interface CfdpStatus {
  transaction_id: number
  source_entity: number
  dest_entity: number
  progress_pct: number
  complete: boolean
}

export interface MFileStatus {
  transaction_id: number
  total_chunks: number
  received_chunks: number
  gaps: [number, number][]
}

export type LinkVariant = 'Aos' | 'Los' | 'Degraded'

export interface LinkStateDto {
  state: LinkVariant
  last_frame_utc: string | null
}

export interface Cop1Status {
  fop1_state: string
  window_occupancy: number
  retransmit_count: number
}

export interface TimeAuthority {
  tai_offset_s: number
  drift_budget_us_per_day: number
  sync_packet_age_ms: number
  time_suspect_seen: boolean
}

export interface LinkHealthSnapshot {
  link: string
  apid: number
  last_hk_utc: string | null
}

export interface WsSnapshot {
  hk: HkSnapshot[]
  events: EventEntry[]
  cfdp: CfdpStatus[]
  mfile: MFileStatus[]
  link: LinkStateDto
  cop1: Cop1Status
  time_auth: TimeAuthority
  links_health: LinkHealthSnapshot[]
}

export interface CfsRosLinkDto {
  packets_routed: number
  apid_rejects: number
  tc_forwarded: number
  uptime_s: number
  cmd_counter: number
  err_counter: number
  last_hk_utc: string | null
}

export interface RosGroundLinkDto {
  session_active: boolean
  signal_strength: number
  last_contact_s: number
  last_hk_utc: string | null
}

export interface FleetDdsLinkDto {
  health_mask: number
  land_age_ms: number
  uav_age_ms: number
  cryo_age_ms: number
  last_hk_utc: string | null
}
