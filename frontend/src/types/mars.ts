export type VehicleType = 'drone' | 'land' | 'cryobot'

export interface MarsVehicle {
  id: string
  type: VehicleType
  label: string
  apid: number
  instanceId: number
  basePosition: [number, number, number]
  battery?: number
  altitude?: number
  heading?: number
  temperature?: number
  drillDepth?: number
  motorRpm?: number
  posX?: number
  posZ?: number
  seqCount: number
  lastPacketAt?: number
}

export interface CommEvent {
  id: string
  fromId: string
  toId: string
  apid: number
  apidName: string
  decodedFields: Record<string, string>
  timestamp: number
  expiresAt: number
}
