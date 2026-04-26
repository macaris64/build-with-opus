import { describe, it, expect } from 'vitest'
import { decodeHkFrame } from '../../lib/hkDecoder'

function u16LE(v: number): number[] {
  const buf = new ArrayBuffer(2)
  new DataView(buf).setUint16(0, v, true)
  return Array.from(new Uint8Array(buf))
}

function u32LE(v: number): number[] {
  const buf = new ArrayBuffer(4)
  new DataView(buf).setUint32(0, v, true)
  return Array.from(new Uint8Array(buf))
}

function f32LE(v: number): number[] {
  const buf = new ArrayBuffer(4)
  new DataView(buf).setFloat32(0, v, true)
  return Array.from(new Uint8Array(buf))
}

function u16LE_val(v: number): number[] {
  const buf = new ArrayBuffer(2)
  new DataView(buf).setUint16(0, v, true)
  return Array.from(new Uint8Array(buf))
}

describe('decodeHkFrame — CDH (0x101)', () => {
  it('decodes mode, cmd_counter, err_counter, uptime', () => {
    const data = [0x01, 0x00, 0x05, 0x00, 0x00, 0x00, ...u32LE(42)]
    const decoded = decodeHkFrame(0x101, data)
    expect(decoded.mode).toBe(1)
    expect(decoded.cmd_counter).toBe(5)
    expect(decoded.uptime_s).toBe(42)
  })
})

describe('decodeHkFrame — COMM (0x120)', () => {
  it('decodes link_state and tx_frames', () => {
    const data = [0x01, 0x00, 0x64, 0x00, 0x07, 0x00, 0x00, 0x00]
    const decoded = decodeHkFrame(0x120, data)
    expect(decoded.link_state).toBe(1)
    expect(decoded.vc0_rate_kbps).toBe(100)
    expect(decoded.tx_frames).toBe(7)
  })
})

describe('decodeHkFrame — Power (0x130)', () => {
  it('decodes bus_voltage and battery_pct', () => {
    const voltage_mv = 28000
    const data = [...u16LE(voltage_mv), ...u16LE(90), ...u16LE(2100), ...u16LE(0)]
    const decoded = decodeHkFrame(0x130, data)
    expect(Number(decoded.bus_voltage_V)).toBeCloseTo(28.0, 1)
    expect(decoded.battery_pct).toBe(90)
  })
})

describe('decodeHkFrame — Fleet monitor (0x160)', () => {
  it('decodes health mask and ages', () => {
    const data = [0x07, ...u32LE(800).reverse(), ...u32LE(900).reverse(), ...u32LE(950).reverse()]
    const decoded = decodeHkFrame(0x160, data)
    expect(decoded.land_alive).toBe('YES')
    expect(decoded.uav_alive).toBe('YES')
    expect(decoded.cryo_alive).toBe('YES')
  })

  it('reports NO when health bit is 0', () => {
    const data = [0x00, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
    const decoded = decodeHkFrame(0x160, data)
    expect(decoded.land_alive).toBe('NO')
    expect(decoded.uav_alive).toBe('NO')
    expect(decoded.cryo_alive).toBe('NO')
  })
})

describe('decodeHkFrame — Mars Rover (0x300)', () => {
  it('decodes x_m, y_m, heading_deg, speed_cm_s', () => {
    const data = [...f32LE(10.5), ...f32LE(-3.2), ...f32LE(270.0), ...u16LE_val(25)]
    const decoded = decodeHkFrame(0x300, data)
    expect(Number(decoded.x_m)).toBeCloseTo(10.5, 1)
    expect(Number(decoded.heading_deg)).toBeCloseTo(270.0, 0)
    expect(decoded.speed_cm_s).toBe(25)
  })

  it('same decoder applies to 0x301 and 0x302', () => {
    const data = [...f32LE(0), ...f32LE(0), ...f32LE(0), 0, 0]
    expect(decodeHkFrame(0x301, data).x_m).toBe('0.00')
    expect(decodeHkFrame(0x302, data).x_m).toBe('0.00')
  })
})

describe('decodeHkFrame — Mars UAV (0x3c0)', () => {
  it('decodes altitude_m, x_m, y_m, battery_pct', () => {
    const data = [...f32LE(55.0), ...f32LE(12.3), ...f32LE(-8.1), ...f32LE(78.5)]
    const decoded = decodeHkFrame(0x3c0, data)
    expect(Number(decoded.altitude_m)).toBeCloseTo(55.0, 1)
    expect(Number(decoded.battery_pct)).toBeCloseTo(78.5, 1)
  })
})

describe('decodeHkFrame — Mars Cryobot (0x400)', () => {
  it('decodes depth_m, drill_rpm, borehole_temp_C (negative)', () => {
    const buf = new ArrayBuffer(2)
    new DataView(buf).setInt16(0, -1800, true)
    const i16bytes = Array.from(new Uint8Array(buf))
    const data = [...f32LE(3.5), ...f32LE(440.0), ...i16bytes]
    const decoded = decodeHkFrame(0x400, data)
    expect(Number(decoded.depth_m)).toBeCloseTo(3.5, 2)
    expect(Number(decoded.borehole_temp_C)).toBeCloseTo(-180.0, 1)
  })
})

describe('decodeHkFrame — ADCS (0x110)', () => {
  it('decodes quaternion and angular rates', () => {
    const data = [
      ...f32LE(0.7071), ...f32LE(0.0), ...f32LE(0.0), ...f32LE(0.7071),
      ...f32LE(0.01),   ...f32LE(0.005), ...f32LE(0.001),
      0, 0, 0, 0,
    ]
    const decoded = decodeHkFrame(0x110, data)
    expect(Number(decoded['q_w'])).toBeCloseTo(0.7071, 3)
    expect(Number(decoded['q_z'])).toBeCloseTo(0.7071, 3)
  })
})

describe('decodeHkFrame — ros2_bridge (0x128)', () => {
  it('decodes packet counters', () => {
    const data = [...u32LE(300), ...u32LE(2), ...u32LE(100), ...u32LE(99), ...u32LE(302), ...u32LE(0)]
    const decoded = decodeHkFrame(0x128, data)
    expect(decoded.packets_routed).toBe(300)
    expect(decoded.tc_forwarded).toBe(100)
  })
})

describe('decodeHkFrame — prx1 (0x129)', () => {
  it('reports session_active YES when byte is non-zero', () => {
    const data = [1, 180, 0, 0, 0, 0, 0, 0, 0, 0]
    const decoded = decodeHkFrame(0x129, data)
    expect(decoded.session_active).toBe('YES')
  })

  it('reports session_active NO when byte is 0', () => {
    const data = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
    const decoded = decodeHkFrame(0x129, data)
    expect(decoded.session_active).toBe('NO')
  })
})

describe('decodeHkFrame — payload (0x140)', () => {
  it('decodes science_mode and frame_count', () => {
    const data = [0x01, 0x00, 0xe8, 0x03, 0x10, 0x00]
    const decoded = decodeHkFrame(0x140, data)
    expect(decoded.science_mode).toBe(1)
    expect(decoded.frame_count).toBe(16)
  })
})

describe('decodeHkFrame — unknown APID', () => {
  it('returns raw_hex dump for unknown APID', () => {
    const decoded = decodeHkFrame(0x999, [0xde, 0xad, 0xbe, 0xef])
    expect(decoded).toHaveProperty('raw_hex')
    expect(String(decoded.raw_hex)).toContain('de')
  })
})
