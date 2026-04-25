import clsx from 'clsx'

type Variant = 'green' | 'amber' | 'red' | 'blue' | 'gray'

interface Props {
  variant: Variant
  label: string
  pulse?: boolean
  className?: string
}

const colors: Record<Variant, string> = {
  green: 'bg-green-500/20 text-green-400 border-green-500/40',
  amber: 'bg-amber-500/20 text-amber-400 border-amber-500/40',
  red:   'bg-red-500/20 text-red-400 border-red-500/40',
  blue:  'bg-blue-500/20 text-blue-400 border-blue-500/40',
  gray:  'bg-gray-500/20 text-gray-400 border-gray-500/40',
}

export function StatusBadge({ variant, label, pulse = false, className }: Props) {
  return (
    <span
      className={clsx(
        'inline-flex items-center gap-1.5 px-2 py-0.5 rounded border text-xs font-bold font-mono',
        colors[variant],
        className,
      )}
    >
      <span
        className={clsx(
          'inline-block w-1.5 h-1.5 rounded-full',
          variant === 'green' && 'bg-green-400',
          variant === 'amber' && 'bg-amber-400',
          variant === 'red' && 'bg-red-400',
          variant === 'blue' && 'bg-blue-400',
          variant === 'gray' && 'bg-gray-400',
          pulse && 'animate-ping-slow',
        )}
      />
      {label}
    </span>
  )
}
