import clsx from 'clsx'
import { SpaceScene } from '../scene/SpaceScene'
import { LinkHealthBar } from '../panels/LinkHealthBar'
import { HkPanel } from '../panels/HkPanel'
import { EventLog } from '../panels/EventLog'
import { TimeDisplay } from '../panels/TimeDisplay'
import { Cop1Status } from '../panels/Cop1Status'
import { CfdpProgress } from '../panels/CfdpProgress'
import { CommandPanel } from '../commands/CommandPanel'
import { useUiStore } from '../store/uiStore'

type Tab = 'hk' | 'events' | 'commands' | 'cfdp'

const TABS: { id: Tab; label: string }[] = [
  { id: 'hk', label: 'TLM' },
  { id: 'events', label: 'EVENTS' },
  { id: 'commands', label: 'CMD' },
  { id: 'cfdp', label: 'CFDP' },
]

export function Layout() {
  const sidebarTab = useUiStore((s) => s.sidebarTab)
  const setSidebarTab = useUiStore((s) => s.setSidebarTab)

  return (
    <div className="flex flex-1 overflow-hidden flex-col lg:flex-row">
      {/* 3D Scene */}
      <div className="flex-1 min-h-0 relative">
        <SpaceScene />
        {/* Scene legend */}
        <div className="absolute bottom-3 left-3 flex flex-col gap-1 pointer-events-none">
          {[
            ['#00ff88', 'Active'],
            ['#ffaa00', 'Degraded'],
            ['#ff2244', 'LOS'],
          ].map(([color, label]) => (
            <div key={label} className="flex items-center gap-1.5">
              <span className="w-4 h-0.5 inline-block" style={{ backgroundColor: color }} />
              <span className="text-xs text-gray-400 font-mono">{label}</span>
            </div>
          ))}
        </div>
        {/* Click hint */}
        <div className="absolute top-3 left-3 text-xs text-gray-600 font-mono pointer-events-none">
          Click node to inspect · Drag to orbit
        </div>
      </div>

      {/* Sidebar */}
      <div className="w-full lg:w-[420px] shrink-0 bg-gray-950 border-t lg:border-t-0 lg:border-l border-gray-800 flex flex-col overflow-hidden">
        {/* Always-visible top panels */}
        <div className="flex flex-col gap-2 p-3 border-b border-gray-800">
          <LinkHealthBar />
          <div className="grid grid-cols-2 gap-2">
            <Cop1Status />
            <TimeDisplay />
          </div>
        </div>

        {/* Tabs */}
        <div className="flex border-b border-gray-800">
          {TABS.map((tab) => (
            <button
              key={tab.id}
              onClick={() => setSidebarTab(tab.id)}
              className={clsx(
                'flex-1 py-2 text-xs font-bold tracking-wider transition-colors',
                sidebarTab === tab.id
                  ? 'text-cyan-400 border-b-2 border-cyan-400 bg-cyan-400/5'
                  : 'text-gray-500 hover:text-gray-300',
              )}
            >
              {tab.label}
            </button>
          ))}
        </div>

        {/* Tab content */}
        <div className="flex-1 overflow-y-auto p-3">
          {sidebarTab === 'hk' && <HkPanel />}
          {sidebarTab === 'events' && <EventLog />}
          {sidebarTab === 'commands' && <CommandPanel />}
          {sidebarTab === 'cfdp' && <CfdpProgress />}
        </div>
      </div>
    </div>
  )
}
