import { useRef } from 'react'
import * as THREE from 'three'
import { useFrame } from '@react-three/fiber'
import { useUiStore } from '../store/uiStore'

const SEMI_A = 14
const SEMI_B = 8
const HEIGHT = 10
const OMEGA = 0.2

export function OrbiterMesh() {
  const groupRef = useRef<THREE.Group>(null)
  const selectNode = useUiStore((s) => s.selectNode)

  useFrame(({ clock }) => {
    const t = clock.getElapsedTime()
    const group = groupRef.current
    if (!group) return
    group.position.x = SEMI_A * Math.cos(OMEGA * t)
    group.position.z = SEMI_B * Math.sin(OMEGA * t)
    group.position.y = HEIGHT + 2 * Math.sin(OMEGA * t * 0.5)
    group.rotation.y = OMEGA * t + Math.PI / 2
  })

  return (
    <group ref={groupRef} onClick={() => selectNode('orbiter')}>
      {/* Main body */}
      <mesh castShadow>
        <boxGeometry args={[0.8, 0.3, 0.8]} />
        <meshStandardMaterial color="#ccccdd" metalness={0.75} roughness={0.25} />
      </mesh>
      {/* Solar panel left */}
      <mesh position={[-1.4, 0, 0]} castShadow>
        <boxGeometry args={[2, 0.04, 0.5]} />
        <meshStandardMaterial color="#1a3a6a" metalness={0.4} roughness={0.6} />
      </mesh>
      {/* Solar panel right */}
      <mesh position={[1.4, 0, 0]} castShadow>
        <boxGeometry args={[2, 0.04, 0.5]} />
        <meshStandardMaterial color="#1a3a6a" metalness={0.4} roughness={0.6} />
      </mesh>
      {/* Antenna dish */}
      <mesh position={[0, 0.3, 0]} rotation={[Math.PI / 6, 0, 0]}>
        <sphereGeometry args={[0.25, 8, 6, 0, Math.PI * 2, 0, Math.PI / 2]} />
        <meshStandardMaterial color="#aaaacc" metalness={0.8} roughness={0.2} side={THREE.DoubleSide} />
      </mesh>
    </group>
  )
}
