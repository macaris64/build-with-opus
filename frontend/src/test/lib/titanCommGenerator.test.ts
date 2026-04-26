import { describe, it, expect } from 'vitest'
import { buildTitanCommEvent, pickRandomTitanPair } from '../../lib/titanCommGenerator'
import type { TitanVehicle } from '../../types/titan'

function makeVehicle(id: string, type: TitanVehicle['type'] = 'uav'): TitanVehicle {
  return {
    id,
    type,
    label: id.toUpperCase(),
    apid: 0x420,
    instanceId: 1,
    basePosition: [0, 12, 0],
    seqCount: 0,
  }
}

describe('pickRandomTitanPair', () => {
  it('returns null for empty array', () => {
    expect(pickRandomTitanPair([])).toBeNull()
  })

  it('returns null for single-vehicle array', () => {
    expect(pickRandomTitanPair([makeVehicle('a')])).toBeNull()
  })

  it('returns distinct from and to with 2+ vehicles', () => {
    const vehicles = [makeVehicle('a'), makeVehicle('b'), makeVehicle('c')]
    const pair = pickRandomTitanPair(vehicles)
    expect(pair).not.toBeNull()
    const [from, to] = pair!
    expect(from.id).not.toBe(to.id)
  })
})

describe('buildTitanCommEvent', () => {
  it('returns event with correct fromId and toId', () => {
    const from = makeVehicle('uav-1', 'uav')
    const to = makeVehicle('rover-1', 'rover')
    const evt = buildTitanCommEvent(from, to)
    expect(evt.fromId).toBe('uav-1')
    expect(evt.toId).toBe('rover-1')
  })

  it('expiresAt is after timestamp', () => {
    const from = makeVehicle('a', 'uav')
    const to = makeVehicle('b', 'rover')
    const evt = buildTitanCommEvent(from, to)
    expect(evt.expiresAt).toBeGreaterThan(evt.timestamp)
  })

  it('UAV event has alt and bat fields', () => {
    const from = makeVehicle('uav', 'uav')
    const to = makeVehicle('rover', 'rover')
    const evt = buildTitanCommEvent(from, to)
    expect(evt.decodedFields).toHaveProperty('alt')
    expect(evt.decodedFields).toHaveProperty('bat')
    expect(evt.apidName).toBe('TITAN UAV HK')
  })

  it('rover event has pos-x and bat fields', () => {
    const from = { ...makeVehicle('r', 'rover'), apid: 0x410 }
    const to = makeVehicle('t', 'uav')
    const evt = buildTitanCommEvent(from, to)
    expect(evt.decodedFields).toHaveProperty('pos-x')
    expect(evt.decodedFields).toHaveProperty('bat')
    expect(evt.apidName).toBe('TITAN ROVER HK')
  })

  it('cryobot event has depth, rpm, temp fields', () => {
    const from = { ...makeVehicle('c', 'cryobot'), apid: 0x430 }
    const to = makeVehicle('t', 'rover')
    const evt = buildTitanCommEvent(from, to)
    expect(evt.decodedFields).toHaveProperty('depth')
    expect(evt.decodedFields).toHaveProperty('rpm')
    expect(evt.decodedFields).toHaveProperty('temp')
    expect(evt.apidName).toBe('TITAN CRYO HK')
  })

  it('consecutive events have distinct ids', () => {
    const from = makeVehicle('a', 'uav')
    const to = makeVehicle('b', 'rover')
    const e1 = buildTitanCommEvent(from, to)
    const e2 = buildTitanCommEvent(from, to)
    expect(e1.id).not.toBe(e2.id)
  })
})
