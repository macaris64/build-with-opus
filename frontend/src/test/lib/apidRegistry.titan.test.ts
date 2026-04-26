import { describe, it, expect } from 'vitest'
import { getApidName, getApidNodeId, APID_NAMES } from '../../lib/apidRegistry'

describe('apidRegistry — Titan APIDs', () => {
  it('maps 0x410 to titan_rover_1', () => {
    expect(getApidName(0x410)).toBe('titan_rover_1')
  })

  it('maps 0x411 to titan_rover_2', () => {
    expect(getApidName(0x411)).toBe('titan_rover_2')
  })

  it('maps 0x412 to titan_rover_3', () => {
    expect(getApidName(0x412)).toBe('titan_rover_3')
  })

  it('maps 0x420 to titan_uav_1', () => {
    expect(getApidName(0x420)).toBe('titan_uav_1')
  })

  it('maps 0x424 to titan_uav_5', () => {
    expect(getApidName(0x424)).toBe('titan_uav_5')
  })

  it('maps 0x430 to titan_cryobot_1', () => {
    expect(getApidName(0x430)).toBe('titan_cryobot_1')
  })

  it('maps 0x433 to titan_cryobot_4', () => {
    expect(getApidName(0x433)).toBe('titan_cryobot_4')
  })

  it('falls back to hex string for unknown APID', () => {
    expect(getApidName(0x999)).toMatch(/APID 0x999/i)
  })

  it('getApidNodeId returns node id for known APID', () => {
    expect(getApidNodeId(0x101)).toBe('orbiter')
    expect(getApidNodeId(0x300)).toBe('rover-land')
  })

  it('getApidNodeId returns null for unknown APID', () => {
    expect(getApidNodeId(0x999)).toBeNull()
  })

  it('APID_NAMES contains all 12 Titan entries', () => {
    const titanApiDs = [
      0x410, 0x411, 0x412,
      0x420, 0x421, 0x422, 0x423, 0x424,
      0x430, 0x431, 0x432, 0x433,
    ]
    for (const apid of titanApiDs) {
      expect(APID_NAMES[apid]).toBeDefined()
    }
  })
})
