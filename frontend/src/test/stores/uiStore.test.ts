import { describe, it, expect, beforeEach } from 'vitest'
import { useUiStore, getCommandState } from '../../store/uiStore'
import type { ActiveView } from '../../store/uiStore'

beforeEach(() => {
  useUiStore.setState({ activeView: 'space', selectedNodeId: null })
})

describe('useUiStore', () => {
  it('setActiveView stores space', () => {
    useUiStore.getState().setActiveView('space')
    expect(useUiStore.getState().activeView).toBe('space')
  })

  it('setActiveView stores mars', () => {
    useUiStore.getState().setActiveView('mars')
    expect(useUiStore.getState().activeView).toBe('mars')
  })

  it('setActiveView stores titan', () => {
    useUiStore.getState().setActiveView('titan')
    expect(useUiStore.getState().activeView).toBe('titan')
  })

  it('ActiveView type accepts all three values', () => {
    const views: ActiveView[] = ['space', 'mars', 'titan']
    for (const v of views) {
      useUiStore.getState().setActiveView(v)
      expect(useUiStore.getState().activeView).toBe(v)
    }
  })

  it('selectNode stores id', () => {
    useUiStore.getState().selectNode('orbiter')
    expect(useUiStore.getState().selectedNodeId).toBe('orbiter')
  })

  it('selectNode(null) clears selection', () => {
    useUiStore.setState({ selectedNodeId: 'some-node' })
    useUiStore.getState().selectNode(null)
    expect(useUiStore.getState().selectedNodeId).toBeNull()
  })

  it('setSidebarTab updates sidebarTab', () => {
    useUiStore.getState().setSidebarTab('events')
    expect(useUiStore.getState().sidebarTab).toBe('events')
    useUiStore.getState().setSidebarTab('hk')
    expect(useUiStore.getState().sidebarTab).toBe('hk')
  })

  it('setCommandState stores state by id', () => {
    useUiStore.getState().setCommandState('cmd-1', { status: 'pending', lastSentAt: '2026-01-01T00:00:00Z', errorMsg: null })
    const { commandStates } = useUiStore.getState()
    expect(commandStates['cmd-1']?.status).toBe('pending')
  })
})

describe('getCommandState', () => {
  it('returns stored state for known id', () => {
    const states: Record<string, import('../../types/commands').CommandState> = {
      'cmd-1': { status: 'accepted', lastSentAt: '2026-01-01T00:00:00Z', errorMsg: null },
    }
    expect(getCommandState(states, 'cmd-1').status).toBe('accepted')
  })

  it('returns default idle state for unknown id', () => {
    const state = getCommandState({}, 'unknown')
    expect(state.status).toBe('idle')
    expect(state.lastSentAt).toBeNull()
    expect(state.errorMsg).toBeNull()
  })
})
