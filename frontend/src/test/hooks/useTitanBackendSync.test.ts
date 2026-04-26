import { describe, it, expect, vi, beforeEach } from 'vitest'
import { renderHook } from '@testing-library/react'

// Mock Zustand stores before importing the hook
vi.mock('../../store/telemetryStore', () => ({
  useTelemetryStore: vi.fn(),
}))
vi.mock('../../store/titanStore', async (importOriginal) => {
  const orig = await importOriginal<typeof import('../../store/titanStore')>()
  return {
    ...orig,
    useTitanStore: vi.fn(),
  }
})

import { useTelemetryStore } from '../../store/telemetryStore'
import { useTitanStore } from '../../store/titanStore'
import { useTitanBackendSync } from '../../hooks/useTitanBackendSync'

function makeHkSnapshot(apid: number, timestamp: string, data: number[]) {
  return { asset: 'test', apid, frames: [{ timestamp_utc: timestamp, data }] }
}

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

describe('useTitanBackendSync', () => {
  const updateVehicle = vi.fn()
  const addComm = vi.fn()
  const addToLog = vi.fn()
  const expireComms = vi.fn()

  beforeEach(() => {
    vi.clearAllMocks()
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    vi.mocked(useTitanStore).mockImplementation((selector: any) =>
      selector({
        hk: [],
        addComm,
        addToLog,
        expireComms,
        updateVehicle,
        vehicles: [
          { id: 'titan-uav-1', type: 'uav', label: 'UAV-T1', apid: 0x420, instanceId: 1, basePosition: [0, 12, 0], seqCount: 5 },
          { id: 'titan-rover-1', type: 'rover', label: 'ROVER-T1', apid: 0x410, instanceId: 1, basePosition: [12, 2, 8], seqCount: 0 },
        ],
      })
    )
  })

  it('calls expireComms on interval', async () => {
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    vi.mocked(useTelemetryStore).mockImplementation((selector: any) =>
      selector({ hk: [] })
    )
    vi.useFakeTimers()
    renderHook(() => useTitanBackendSync())
    vi.advanceTimersByTime(600)
    expect(expireComms).toHaveBeenCalled()
    vi.useRealTimers()
  })

  it('updates vehicle when Titan APID frame arrives', async () => {
    const uavPayload = [
      ...f32LE(13.5),  // altitude
      ...f32LE(1.0),   // x
      ...f32LE(-1.0),  // y
      ...f32LE(82.0),  // battery
    ]
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    vi.mocked(useTelemetryStore).mockImplementation((selector: any) =>
      selector({ hk: [makeHkSnapshot(0x420, '2026-01-01T00:00:01Z', uavPayload)] })
    )
    renderHook(() => useTitanBackendSync())
    expect(updateVehicle).toHaveBeenCalledWith(
      'titan-uav-1',
      expect.objectContaining({ seqCount: 6 }),
    )
  })

  it('ignores non-Titan APIDs', () => {
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    vi.mocked(useTelemetryStore).mockImplementation((selector: any) =>
      selector({ hk: [makeHkSnapshot(0x101, '2026-01-01T00:00:01Z', [0, 0, 0, 0])] })
    )
    renderHook(() => useTitanBackendSync())
    expect(updateVehicle).not.toHaveBeenCalled()
  })

  it('skips duplicate timestamps', () => {
    const payload = [...f32LE(13.0), ...f32LE(0), ...f32LE(0), ...f32LE(80.0)]
    const snap = makeHkSnapshot(0x420, '2026-01-01T00:00:01Z', payload)
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    vi.mocked(useTelemetryStore).mockImplementation((selector: any) =>
      selector({ hk: [snap] })
    )
    const { rerender } = renderHook(() => useTitanBackendSync())
    rerender()
    // updateVehicle should only be called once (first render), not again on rerender with same ts
    expect(updateVehicle).toHaveBeenCalledTimes(1)
  })

  it('routes Titan rover APID 0x410 to titan-rover-1', () => {
    const roverPayload = [...f32LE(12.0), ...f32LE(8.0), ...f32LE(90.0), ...u16LE(80)]
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    vi.mocked(useTelemetryStore).mockImplementation((selector: any) =>
      selector({ hk: [makeHkSnapshot(0x410, '2026-01-01T00:00:02Z', roverPayload)] })
    )
    renderHook(() => useTitanBackendSync())
    expect(updateVehicle).toHaveBeenCalledWith('titan-rover-1', expect.any(Object))
  })
})
