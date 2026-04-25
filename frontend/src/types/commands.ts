export interface CommandDef {
  id: string
  name: string
  target: string
  apid: number
  funcCode: number
  description: string
  isSafetyInterlocked: boolean
  category: 'orbiter' | 'rover'
}

export type CommandStatus = 'idle' | 'pending' | 'accepted' | 'rejected'

export interface CommandState {
  status: CommandStatus
  lastSentAt: string | null
  errorMsg: string | null
}
