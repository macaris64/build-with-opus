import { describe, it, expect, beforeEach } from 'vitest'
import { render, screen } from '@testing-library/react'
import { useTitanStore } from '../../store/titanStore'
import { TitanPacketPanel } from '../../panels/TitanPacketPanel'

function resetStore() {
  useTitanStore.setState({ packetLog: [], activeComms: [], selectedVehicleId: null })
}

function makeEvent(id: string) {
  return {
    id,
    fromId: 'titan-uav-1',
    toId: 'titan-rover-1',
    apid: 0x420,
    apidName: 'TITAN UAV HK',
    decodedFields: { alt: '13.0 m', bat: '80 %' },
    timestamp: 1_700_000_000_000,
    expiresAt: 1_700_000_002_200,
  }
}

describe('TitanPacketPanel', () => {
  beforeEach(() => resetStore())

  it('shows placeholder when packetLog is empty', () => {
    render(<TitanPacketPanel />)
    expect(screen.getByText(/Awaiting inter-vehicle transmissions/i)).toBeInTheDocument()
  })

  it('shows packet count in header', () => {
    useTitanStore.setState({ packetLog: [makeEvent('e1'), makeEvent('e2')] })
    render(<TitanPacketPanel />)
    expect(screen.getByText('2 pkts')).toBeInTheDocument()
  })

  it('renders the TITAN PACKET LOG header', () => {
    render(<TitanPacketPanel />)
    expect(screen.getByText('TITAN PACKET LOG')).toBeInTheDocument()
  })

  it('shows vehicle labels for log entries', () => {
    useTitanStore.setState({ packetLog: [makeEvent('e1')] })
    render(<TitanPacketPanel />)
    expect(screen.getByText('UAV-T1')).toBeInTheDocument()
  })

  it('shows the APID hex in the log entry', () => {
    useTitanStore.setState({ packetLog: [makeEvent('e1')] })
    render(<TitanPacketPanel />)
    expect(screen.getByText('0x420')).toBeInTheDocument()
  })

  it('shows legend badges: UAV, ROVER, CRYOBOT', () => {
    render(<TitanPacketPanel />)
    expect(screen.getByText('UAV')).toBeInTheDocument()
    expect(screen.getByText('ROVER')).toBeInTheDocument()
    expect(screen.getByText('CRYOBOT')).toBeInTheDocument()
  })

  it('shows raw vehicle id when vehicle not in store', () => {
    const unknownEvt = {
      id: 'e-unknown',
      fromId: 'ghost-vehicle',
      toId: 'titan-rover-1',
      apid: 0x420,
      apidName: 'TITAN UAV HK',
      decodedFields: { alt: '12.0 m' },
      timestamp: 1_700_000_000_000,
      expiresAt: 1_700_000_002_200,
    }
    useTitanStore.setState({ packetLog: [unknownEvt] })
    render(<TitanPacketPanel />)
    expect(screen.getByText('ghost-vehicle')).toBeInTheDocument()
  })

  it('renders decoded field string in the row', () => {
    useTitanStore.setState({ packetLog: [makeEvent('e1')] })
    render(<TitanPacketPanel />)
    expect(screen.getByText(/alt=13\.0 m/)).toBeInTheDocument()
  })
})
