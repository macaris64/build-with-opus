import { describe, it, expect } from 'vitest'
import { decodeHkFrame } from '../../lib/hkDecoder'

function f32LE(v: number): number[] {
  const buf = new ArrayBuffer(4)
  new DataView(buf).setFloat32(0, v, true)
  return Array.from(new Uint8Array(buf))
}

function u16LE(v: number): number[] {
  const buf = new ArrayBuffer(2)
  new DataView(buf).setUint16(0, v, true)
  return Array.from(new Uint8Array(buf))
}

function i16LE(v: number): number[] {
  const buf = new ArrayBuffer(2)
  new DataView(buf).setInt16(0, v, true)
  return Array.from(new Uint8Array(buf))
}

describe('decodeHkFrame — Titan Rovers (0x410–0x412)', () => {
  const payload = [
    ...f32LE(12.5),   // x_m
    ...f32LE(-3.2),   // y_m
    ...f32LE(180.0),  // heading_deg
    ...u16LE(82),     // battery_pct
  ]

  for (const apid of [0x410, 0x411, 0x412]) {
    it(`APID 0x${apid.toString(16)} decodes rover fields`, () => {
      const decoded = decodeHkFrame(apid, payload)
      expect(Number(decoded.x_m)).toBeCloseTo(12.5, 1)
      expect(Number(decoded.y_m)).toBeCloseTo(-3.2, 1)
      expect(Number(decoded.heading_deg)).toBeCloseTo(180.0, 0)
      expect(decoded.battery_pct).toBe(82)
    })
  }

  it('all-zero payload returns zero values without throwing', () => {
    const zeros = new Array(14).fill(0)
    const decoded = decodeHkFrame(0x410, zeros)
    expect(Number(decoded.x_m)).toBe(0)
    expect(Number(decoded.y_m)).toBe(0)
    expect(decoded.battery_pct).toBe(0)
  })
})

describe('decodeHkFrame — Titan UAVs (0x420–0x424)', () => {
  const payload = [
    ...f32LE(13.4),  // altitude_m
    ...f32LE(2.1),   // x_m
    ...f32LE(-1.8),  // y_m
    ...f32LE(78.5),  // battery_pct
  ]

  for (const apid of [0x420, 0x421, 0x422, 0x423, 0x424]) {
    it(`APID 0x${apid.toString(16)} decodes UAV fields`, () => {
      const decoded = decodeHkFrame(apid, payload)
      expect(Number(decoded.altitude_m)).toBeCloseTo(13.4, 1)
      expect(Number(decoded.x_m)).toBeCloseTo(2.1, 1)
      expect(Number(decoded.y_m)).toBeCloseTo(-1.8, 1)
      expect(Number(decoded.battery_pct)).toBeCloseTo(78.5, 1)
    })
  }
})

describe('decodeHkFrame — Titan Cryobots (0x430–0x433)', () => {
  const posTemp = [
    ...f32LE(12.345),  // depth_m
    ...f32LE(420.0),   // drill_rpm
    ...i16LE(-1790),   // temp × 10 = −179.0 °C
    0, 0,              // padding
  ]
  const negTemp = [
    ...f32LE(0.0),
    ...f32LE(350.0),
    ...i16LE(-1832),   // −183.2 °C
    0, 0,
  ]

  for (const apid of [0x430, 0x431, 0x432, 0x433]) {
    it(`APID 0x${apid.toString(16)} decodes cryobot fields (negative temp)`, () => {
      const decoded = decodeHkFrame(apid, posTemp)
      expect(Number(decoded.depth_m)).toBeCloseTo(12.345, 2)
      expect(Number(decoded.drill_rpm)).toBeCloseTo(420, 0)
      expect(Number(decoded.borehole_temp_C)).toBeCloseTo(-179.0, 1)
    })
  }

  it('decodes very cold temperature correctly (−183.2 °C)', () => {
    const decoded = decodeHkFrame(0x430, negTemp)
    expect(Number(decoded.borehole_temp_C)).toBeCloseTo(-183.2, 1)
  })
})
