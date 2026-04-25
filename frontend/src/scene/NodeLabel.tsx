import { Html } from '@react-three/drei'

interface Props {
  position: [number, number, number]
  label: string
  apidHex: string
  color: string
}

export function NodeLabel({ position, label, apidHex, color }: Props) {
  return (
    <Html position={position} center occlude={false}>
      <div
        style={{ borderColor: color }}
        className="pointer-events-none select-none whitespace-nowrap text-[10px] font-mono px-1.5 py-0.5 rounded border bg-gray-950/80 backdrop-blur-sm"
      >
        <span style={{ color }} className="font-bold">{apidHex}</span>
        <span className="text-gray-400 mx-1">·</span>
        <span className="text-gray-300">{label}</span>
      </div>
    </Html>
  )
}
