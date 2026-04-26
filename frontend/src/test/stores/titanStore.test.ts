import { describe, it, expect, beforeEach } from 'vitest'
import { useTitanStore, TITAN_VEHICLE_COLOR, TITAN_APID_TO_VEHICLE_ID } from '../../store/titanStore'

function resetStore() {
  // Reset mutable state; vehicles array is unchanged by tests that don't modify it
  useTitanStore.setState({
    activeComms: [],
    packetLog: [],
    selectedVehicleId: null,
  })
  // Restore vehicle telemetry fields to pristine state
  useTitanStore.setState((s) => ({
    vehicles: s.vehicles.map((v) => ({
      ...v,
      altitude: undefined,
      battery: undefined,
      heading: undefined,
      temperature: undefined,
      drillDepth: undefined,
      motorRpm: undefined,
      posX: undefined,
      posZ: undefined,
      seqCount: 0,
      lastPacketAt: undefined,
    })),
  }))
}

function makeEvent(id = 'e1') {
  return {
    id,
    fromId: 'titan-uav-1',
    toId: 'titan-rover-1',
    apid: 0x420,
    apidName: 'TITAN UAV HK',
    decodedFields: { alt: '12.0 m', bat: '80 %' },
    timestamp: Date.now(),
    expiresAt: Date.now() + 2200,
  }
}

describe('useTitanStore', () => {
  beforeEach(() => resetStore())

  it('has 12 initial vehicles', () => {
    const { vehicles } = useTitanStore.getState()
    expect(vehicles).toHaveLength(12)
  })

  it('has 5 UAVs, 3 rovers, 4 cryobots', () => {
    const { vehicles } = useTitanStore.getState()
    expect(vehicles.filter((v) => v.type === 'uav')).toHaveLength(5)
    expect(vehicles.filter((v) => v.type === 'rover')).toHaveLength(3)
    expect(vehicles.filter((v) => v.type === 'cryobot')).toHaveLength(4)
  })

  it('addComm appends to activeComms', () => {
    useTitanStore.getState().addComm(makeEvent())
    expect(useTitanStore.getState().activeComms).toHaveLength(1)
  })

  it('addToLog prepends and caps at 50', () => {
    const { addToLog } = useTitanStore.getState()
    for (let i = 0; i < 55; i++) addToLog(makeEvent(`e${i}`))
    expect(useTitanStore.getState().packetLog).toHaveLength(50)
    expect(useTitanStore.getState().packetLog[0]!.id).toBe('e54')
  })

  it('expireComms removes expired events', () => {
    const expired = { ...makeEvent('old'), expiresAt: Date.now() - 1 }
    const live = { ...makeEvent('new'), expiresAt: Date.now() + 5000 }
    useTitanStore.setState({ activeComms: [expired, live] })
    useTitanStore.getState().expireComms()
    const comms = useTitanStore.getState().activeComms
    expect(comms).toHaveLength(1)
    expect(comms[0]!.id).toBe('new')
  })

  it('updateVehicle patches correct vehicle and leaves others unchanged', () => {
    useTitanStore.getState().updateVehicle('titan-uav-1', { altitude: 13.5, battery: 90 })
    const { vehicles } = useTitanStore.getState()
    const uav1 = vehicles.find((v) => v.id === 'titan-uav-1')!
    expect(uav1.altitude).toBe(13.5)
    expect(uav1.battery).toBe(90)
    // Other vehicle unchanged
    const uav2 = vehicles.find((v) => v.id === 'titan-uav-2')!
    expect(uav2.altitude).toBeUndefined()
  })

  it('selectVehicle sets selectedVehicleId', () => {
    useTitanStore.getState().selectVehicle('titan-cryo-1')
    expect(useTitanStore.getState().selectedVehicleId).toBe('titan-cryo-1')
  })

  it('selectVehicle(null) clears selection', () => {
    useTitanStore.setState({ selectedVehicleId: 'titan-rover-1' })
    useTitanStore.getState().selectVehicle(null)
    expect(useTitanStore.getState().selectedVehicleId).toBeNull()
  })

  it('TITAN_VEHICLE_COLOR has entries for all three types', () => {
    expect(TITAN_VEHICLE_COLOR.uav).toBeDefined()
    expect(TITAN_VEHICLE_COLOR.rover).toBeDefined()
    expect(TITAN_VEHICLE_COLOR.cryobot).toBeDefined()
  })

  it('TITAN_APID_TO_VEHICLE_ID maps 12 APIDs', () => {
    expect(Object.keys(TITAN_APID_TO_VEHICLE_ID)).toHaveLength(12)
    expect(TITAN_APID_TO_VEHICLE_ID[0x420]).toBe('titan-uav-1')
    expect(TITAN_APID_TO_VEHICLE_ID[0x410]).toBe('titan-rover-1')
    expect(TITAN_APID_TO_VEHICLE_ID[0x433]).toBe('titan-cryo-4')
  })
})
