function readU16LE(data: number[], offset: number): number {
  if (offset + 1 >= data.length) return 0
  return ((data[offset + 1] ?? 0) << 8) | (data[offset] ?? 0)
}

function readU32LE(data: number[], offset: number): number {
  if (offset + 3 >= data.length) return 0
  return (
    ((data[offset + 3] ?? 0) * 0x1000000) +
    ((data[offset + 2] ?? 0) << 16) +
    ((data[offset + 1] ?? 0) << 8) +
    (data[offset] ?? 0)
  )
}

function readF32LE(data: number[], offset: number): number {
  if (offset + 3 >= data.length) return 0
  const buf = new ArrayBuffer(4)
  const view = new DataView(buf)
  for (let i = 0; i < 4; i++) view.setUint8(i, data[offset + i] ?? 0)
  return view.getFloat32(0, true)
}

function readU32BE(data: number[], offset: number): number {
  if (offset + 3 >= data.length) return 0
  return (
    ((data[offset] ?? 0) * 0x1000000) +
    ((data[offset + 1] ?? 0) << 16) +
    ((data[offset + 2] ?? 0) << 8) +
    (data[offset + 3] ?? 0)
  )
}

function readF64BE(data: number[], offset: number): number {
  if (offset + 7 >= data.length) return 0
  const buf = new ArrayBuffer(8)
  const view = new DataView(buf)
  for (let i = 0; i < 8; i++) view.setUint8(7 - i, data[offset + i] ?? 0)
  return view.getFloat64(0, false)
}

function hexDump(data: number[]): string {
  return data.map((b) => b.toString(16).padStart(2, '0')).join(' ')
}

export type DecodedHk = Record<string, string | number>

export function decodeHkFrame(apid: number, data: number[]): DecodedHk {
  switch (apid) {
    case 0x101:
      return {
        mode: readU16LE(data, 0),
        cmd_counter: readU16LE(data, 2),
        err_counter: readU16LE(data, 4),
        uptime_s: readU32LE(data, 6),
      }
    case 0x110:
      return {
        'q_w': readF32LE(data, 0).toFixed(4),
        'q_x': readF32LE(data, 4).toFixed(4),
        'q_y': readF32LE(data, 8).toFixed(4),
        'q_z': readF32LE(data, 12).toFixed(4),
        'ω_x rad/s': readF32LE(data, 16).toFixed(4),
        'ω_y rad/s': readF32LE(data, 20).toFixed(4),
        'ω_z rad/s': readF32LE(data, 24).toFixed(4),
        cmd_counter: readU16LE(data, 28),
        err_counter: readU16LE(data, 30),
      }
    case 0x120:
      return {
        link_state: readU16LE(data, 0),
        vc0_rate_kbps: readU16LE(data, 2),
        tx_frames: readU16LE(data, 4),
        err_counter: readU16LE(data, 6),
      }
    case 0x128:
      return {
        packets_routed: readU32LE(data, 0),
        apid_rejects: readU32LE(data, 4),
        tc_forwarded: readU32LE(data, 8),
        uptime_s: readU32LE(data, 12),
        cmd_counter: readU32LE(data, 16),
        err_counter: readU32LE(data, 20),
      }
    case 0x129: {
      const active = (data[0] ?? 0) !== 0
      return {
        session_active: active ? 'YES' : 'NO',
        signal_strength: data[1] ?? 0,
        last_contact_s: readF64BE(data, 2).toFixed(3),
      }
    }
    case 0x130: {
      const rawV = readU16LE(data, 0)
      return {
        bus_voltage_V: (rawV / 1000).toFixed(3),
        battery_pct: readU16LE(data, 2),
        solar_current_mA: readU16LE(data, 4),
        power_mode: readU16LE(data, 6),
      }
    }
    case 0x140:
      return {
        science_mode: readU16LE(data, 0),
        exposure_ms: readU16LE(data, 2),
        frame_count: readU16LE(data, 4),
      }
    case 0x160: {
      const mask = data[0] ?? 0
      return {
        land_alive: (mask & 1) ? 'YES' : 'NO',
        uav_alive: (mask & 2) ? 'YES' : 'NO',
        cryo_alive: (mask & 4) ? 'YES' : 'NO',
        land_age_ms: readU32BE(data, 1),
        uav_age_ms: readU32BE(data, 5),
        cryo_age_ms: readU32BE(data, 9),
      }
    }
    case 0x300:
    case 0x301:
    case 0x302:
      return {
        x_m: readF32LE(data, 0).toFixed(2),
        y_m: readF32LE(data, 4).toFixed(2),
        heading_deg: readF32LE(data, 8).toFixed(1),
        speed_cm_s: readU16LE(data, 12),
      }
    case 0x3c0:
    case 0x3c1:
    case 0x3c2:
    case 0x3c3:
    case 0x3c4:
      return {
        altitude_m: readF32LE(data, 0).toFixed(2),
        x_m: readF32LE(data, 4).toFixed(2),
        y_m: readF32LE(data, 8).toFixed(2),
        battery_pct: readF32LE(data, 12).toFixed(1),
      }
    case 0x400: {
      const tempRaw = readU16LE(data, 8)
      const temp = (tempRaw > 32767 ? tempRaw - 65536 : tempRaw) / 10
      return {
        depth_m: readF32LE(data, 0).toFixed(3),
        drill_rpm: readF32LE(data, 4).toFixed(0),
        borehole_temp_C: temp.toFixed(1),
      }
    }
    default:
      return { raw_hex: hexDump(data.slice(0, 32)) }
  }
}
