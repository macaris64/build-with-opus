export const APID_NAMES: Record<number, string> = {
  0x100: 'sample_app',
  0x101: 'orbiter_cdh',
  0x110: 'orbiter_adcs',
  0x111: 'orbiter_adcs_wheels',
  0x120: 'orbiter_comm',
  0x128: 'ros2_bridge',
  0x129: 'prx1_link_state',
  0x130: 'orbiter_power',
  0x140: 'orbiter_payload',
  0x160: 'fleet_monitor',
  0x280: 'mcu_payload_gw',
  0x290: 'mcu_rwa_gw',
  0x2a0: 'mcu_eps_gw',
  0x300: 'rover_land',
  0x3c0: 'rover_uav',
  0x400: 'rover_cryobot',
}

export const APID_NODE_IDS: Record<number, string> = {
  0x101: 'orbiter',
  0x110: 'orbiter',
  0x120: 'orbiter',
  0x128: 'orbiter',
  0x129: 'rover-uav',
  0x130: 'orbiter',
  0x140: 'orbiter',
  0x160: 'fleet',
  0x300: 'rover-land',
  0x3c0: 'rover-uav',
  0x400: 'rover-cryobot',
}

export function getApidName(apid: number): string {
  return APID_NAMES[apid] ?? `APID 0x${apid.toString(16).toUpperCase().padStart(3, '0')}`
}

export function getApidNodeId(apid: number): string | null {
  return APID_NODE_IDS[apid] ?? null
}
