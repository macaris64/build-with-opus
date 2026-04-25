import { COMMANDS } from './commandDefs'
import { CommandCard } from './CommandCard'

export function CommandPanel() {
  const orbiter = COMMANDS.filter((c) => c.category === 'orbiter')
  const rover = COMMANDS.filter((c) => c.category === 'rover')

  return (
    <div className="flex flex-col gap-3">
      <div>
        <p className="text-xs font-bold text-gray-500 uppercase tracking-widest mb-2">Orbiter</p>
        <div className="flex flex-col gap-2">
          {orbiter.map((c) => <CommandCard key={c.id} def={c} />)}
        </div>
      </div>
      <div>
        <p className="text-xs font-bold text-gray-500 uppercase tracking-widest mb-2">Rovers</p>
        <div className="flex flex-col gap-2">
          {rover.map((c) => <CommandCard key={c.id} def={c} />)}
        </div>
      </div>
    </div>
  )
}
