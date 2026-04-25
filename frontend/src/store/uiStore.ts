import { create } from 'zustand'
import type { CommandState } from '../types/commands'

type SidebarTab = 'hk' | 'events' | 'commands' | 'cfdp'

interface UiStore {
  selectedNodeId: string | null
  sidebarTab: SidebarTab
  commandStates: Record<string, CommandState>
  selectNode: (id: string | null) => void
  setSidebarTab: (tab: SidebarTab) => void
  setCommandState: (id: string, state: CommandState) => void
}

const defaultCommandState: CommandState = { status: 'idle', lastSentAt: null, errorMsg: null }

export const useUiStore = create<UiStore>((set) => ({
  selectedNodeId: null,
  sidebarTab: 'hk',
  commandStates: {},

  selectNode: (id) => set({ selectedNodeId: id }),
  setSidebarTab: (tab) => set({ sidebarTab: tab }),
  setCommandState: (id, state) =>
    set((prev) => ({ commandStates: { ...prev.commandStates, [id]: state } })),
}))

export function getCommandState(
  states: Record<string, CommandState>,
  id: string,
): CommandState {
  return states[id] ?? defaultCommandState
}
