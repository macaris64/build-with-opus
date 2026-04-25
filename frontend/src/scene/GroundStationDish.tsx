import { useRef } from 'react'
import * as THREE from 'three'
import { useFrame } from '@react-three/fiber'
import { useTelemetryStore } from '../store/telemetryStore'
import { useUiStore } from '../store/uiStore'

export function GroundStationDish() {
  const dishRef = useRef<THREE.Group>(null)
  const linkState = useTelemetryStore((s) => s.link.state)
  const selectNode = useUiStore((s) => s.selectNode)
  const selectedId = useUiStore((s) => s.selectedNodeId)

  useFrame(({ clock }) => {
    const dish = dishRef.current
    if (!dish) return
    const t = clock.getElapsedTime()
    if (linkState === 'Aos') {
      dish.rotation.y = t * 0.05
    } else if (linkState === 'Degraded') {
      dish.rotation.y = Math.sin(t * 0.8) * 0.3
    }
  })

  const isSelected = selectedId === 'ground-station'

  return (
    <group
      position={[-8, 0, 6]}
      onClick={(e) => { e.stopPropagation(); selectNode('ground-station') }}
    >
      {/* Mount pole */}
      <mesh position={[0, 1, 0]} castShadow>
        <cylinderGeometry args={[0.08, 0.1, 2, 8]} />
        <meshStandardMaterial color="#777788" metalness={0.6} roughness={0.4} />
      </mesh>
      {/* Rotating dish assembly */}
      <group ref={dishRef} position={[0, 2.2, 0]}>
        <mesh rotation={[-Math.PI / 3, 0, 0]} castShadow>
          <sphereGeometry args={[1.2, 12, 8, 0, Math.PI * 2, 0, Math.PI / 2]} />
          <meshStandardMaterial color="#aaaacc" metalness={0.8} roughness={0.15} side={THREE.DoubleSide} />
        </mesh>
        <mesh position={[0, 0.2, 0.6]}>
          <cylinderGeometry args={[0.04, 0.04, 0.5, 6]} />
          <meshStandardMaterial color="#cccccc" metalness={0.7} roughness={0.3} />
        </mesh>
      </group>
      {isSelected && (
        <mesh position={[0, 1.5, 0]}>
          <sphereGeometry args={[2, 16, 16]} />
          <meshBasicMaterial color="#00ff88" transparent opacity={0.06} wireframe />
        </mesh>
      )}
    </group>
  )
}
