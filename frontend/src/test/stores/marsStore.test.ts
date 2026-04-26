import { describe, it, expect, beforeEach } from 'vitest'
import { useMarsStore, VEHICLE_COLOR, APID_TO_VEHICLE_ID } from '../../store/marsStore'

function resetStore() {
  useMarsStore.setState({
    activeComms: [],
    packetLog: [],
    selectedVehicleId: null,
  })
  useMarsStore.setState((s) => ({
    vehicles: s.vehicles.map((v) => ({
      ...v,
      altitude: undefined,
      battery: undefined,
      heading: undefined,
      seqCount: 0,
      lastPacketAt: undefined,
    })),
  }))
}

function makeEvent(id = 'e1') {
  return {
    id,
    fromId: 'drone-1',
    toId: 'rover-1',
    apid: 0x3c0,
    apidName: 'UAV HK',
    decodedFields: { alt: '50.0 m', bat: '85 %' },
    timestamp: Date.now(),
    expiresAt: Date.now() + 2200,
  }
}

describe('useMarsStore', () => {
  beforeEach(() => resetStore())

  it('has 9 initial vehicles', () => {
    expect(useMarsStore.getState().vehicles).toHaveLength(9)
  })

  it('addComm appends to activeComms', () => {
    useMarsStore.getState().addComm(makeEvent())
    expect(useMarsStore.getState().activeComms).toHaveLength(1)
  })

  it('addToLog prepends and caps at 50', () => {
    const { addToLog } = useMarsStore.getState()
    for (let i = 0; i < 55; i++) addToLog(makeEvent(`e${i}`))
    expect(useMarsStore.getState().packetLog).toHaveLength(50)
    expect(useMarsStore.getState().packetLog[0]!.id).toBe('e54')
  })

  it('expireComms removes expired events', () => {
    const expired = { ...makeEvent('old'), expiresAt: Date.now() - 1 }
    const live = { ...makeEvent('new'), expiresAt: Date.now() + 5000 }
    useMarsStore.setState({ activeComms: [expired, live] })
    useMarsStore.getState().expireComms()
    const comms = useMarsStore.getState().activeComms
    expect(comms).toHaveLength(1)
    expect(comms[0]!.id).toBe('new')
  })

  it('updateVehicle patches correct vehicle only', () => {
    useMarsStore.getState().updateVehicle('drone-1', { altitude: 55.5, battery: 92 })
    const { vehicles } = useMarsStore.getState()
    const d1 = vehicles.find((v) => v.id === 'drone-1')!
    expect(d1.altitude).toBe(55.5)
    const d2 = vehicles.find((v) => v.id === 'drone-2')!
    expect(d2.altitude).toBeUndefined()
  })

  it('selectVehicle sets selectedVehicleId', () => {
    useMarsStore.getState().selectVehicle('cryobot-1')
    expect(useMarsStore.getState().selectedVehicleId).toBe('cryobot-1')
  })

  it('selectVehicle(null) clears', () => {
    useMarsStore.setState({ selectedVehicleId: 'drone-1' })
    useMarsStore.getState().selectVehicle(null)
    expect(useMarsStore.getState().selectedVehicleId).toBeNull()
  })

  it('VEHICLE_COLOR has drone, land, cryobot entries', () => {
    expect(VEHICLE_COLOR.drone).toBeDefined()
    expect(VEHICLE_COLOR.land).toBeDefined()
    expect(VEHICLE_COLOR.cryobot).toBeDefined()
  })

  it('APID_TO_VEHICLE_ID maps drone-1 to 0x3c0', () => {
    expect(APID_TO_VEHICLE_ID[0x3c0]).toBe('drone-1')
    expect(APID_TO_VEHICLE_ID[0x300]).toBe('rover-1')
    expect(APID_TO_VEHICLE_ID[0x400]).toBe('cryobot-1')
  })
})
