import { create } from 'zustand'
import type { CommandState } from '../types/commands'

type SidebarTab = 'hk' | 'events' | 'commands' | 'cfdp'
export type ActiveView = 'space' | 'mars' | 'titan'

interface UiStore {
  selectedNodeId: string | null
  sidebarTab: SidebarTab
  commandStates: Record<string, CommandState>
  activeView: ActiveView
  selectNode: (id: string | null) => void
  setSidebarTab: (tab: SidebarTab) => void
  setCommandState: (id: string, state: CommandState) => void
  setActiveView: (view: ActiveView) => void
}

const defaultCommandState: CommandState = { status: 'idle', lastSentAt: null, errorMsg: null }

export const useUiStore = create<UiStore>((set) => ({
  selectedNodeId: null,
  sidebarTab: 'hk',
  commandStates: {},
  activeView: 'space',

  selectNode: (id) => set({ selectedNodeId: id }),
  setSidebarTab: (tab) => set({ sidebarTab: tab }),
  setCommandState: (id, state) =>
    set((prev) => ({ commandStates: { ...prev.commandStates, [id]: state } })),
  setActiveView: (view) => set({ activeView: view }),
}))

export function getCommandState(
  states: Record<string, CommandState>,
  id: string,
): CommandState {
  return states[id] ?? defaultCommandState
}
