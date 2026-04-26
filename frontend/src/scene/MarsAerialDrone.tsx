import { useRef } from 'react'
import * as THREE from 'three'
import { useFrame } from '@react-three/fiber'
import { useMarsStore, VEHICLE_COLOR } from '../store/marsStore'
import { MarsVehicleInfoBox } from './MarsVehicleInfoBox'
import type { MarsVehicle } from '../types/mars'

interface Props {
  vehicle: MarsVehicle
  hoverSpeed: number
  hoverPhase: number
  orbitRadius: number
  orbitSpeed: number
  orbitPhaseX: number
  orbitPhaseZ: number
}

export function MarsAerialDrone({ vehicle, hoverSpeed, hoverPhase, orbitRadius, orbitSpeed, orbitPhaseX, orbitPhaseZ }: Props) {
  const groupRef = useRef<THREE.Group>(null)
  const rotorRef = useRef<THREE.Group>(null)
  const selectVehicle = useMarsStore((s) => s.selectVehicle)
  const selectedId = useMarsStore((s) => s.selectedVehicleId)
  const isSelected = selectedId === vehicle.id
  const color = VEHICLE_COLOR.drone

  const [baseX, baseY, baseZ] = vehicle.basePosition

  useFrame(({ clock }, delta) => {
    const g = groupRef.current
    const r = rotorRef.current
    if (!g) return
    const t = clock.elapsedTime
    g.position.y = baseY + Math.sin(t * hoverSpeed + hoverPhase) * 0.4
    g.position.x = baseX + Math.sin(t * orbitSpeed + orbitPhaseX) * orbitRadius
    g.position.z = baseZ + Math.cos(t * orbitSpeed + orbitPhaseZ) * orbitRadius
    g.rotation.y += delta * 0.3
    if (r) r.rotation.y += delta * 20
  })

  return (
    <group
      ref={groupRef}
      position={[baseX, baseY, baseZ]}
      onClick={(e) => { e.stopPropagation(); selectVehicle(isSelected ? null : vehicle.id) }}
    >
      {/* Fuselage */}
      <mesh castShadow>
        <coneGeometry args={[0.3, 0.6, 4]} />
        <meshStandardMaterial color="#667799" roughness={0.4} metalness={0.5} />
      </mesh>

      {/* Rotor group */}
      <group ref={rotorRef}>
        {([[0.5, 0.12, 0.5], [-0.5, 0.12, 0.5], [0.5, 0.12, -0.5], [-0.5, 0.12, -0.5]] as [number, number, number][]).map(([x, y, z], i) => (
          <mesh key={i} position={[x, y, z]} rotation={[Math.PI / 2, 0, 0]} castShadow>
            <cylinderGeometry args={[0.22, 0.22, 0.04, 8]} />
            <meshStandardMaterial color={color} roughness={0.2} metalness={0.7} emissive={color} emissiveIntensity={0.15} />
          </mesh>
        ))}
      </group>

      {/* Rotor arm spars */}
      {([[0.5, 0.12, 0.5], [-0.5, 0.12, 0.5], [0.5, 0.12, -0.5], [-0.5, 0.12, -0.5]] as [number, number, number][]).map(([x, y, z], i) => (
        <mesh key={`arm-${i}`} position={[x / 2, y, z / 2]}>
          <boxGeometry args={[Math.abs(x), 0.03, 0.03]} />
          <meshStandardMaterial color="#334455" roughness={0.5} />
        </mesh>
      ))}

      {isSelected && (
        <>
          <mesh rotation={[Math.PI / 2, 0, 0]} position={[0, -0.4, 0]}>
            <torusGeometry args={[1.0, 0.04, 8, 64]} />
            <meshBasicMaterial color={color} transparent opacity={0.75} />
          </mesh>
          <mesh rotation={[Math.PI / 2, 0, 0]} position={[0, -0.4, 0]}>
            <torusGeometry args={[1.3, 0.02, 6, 64]} />
            <meshBasicMaterial color={color} transparent opacity={0.3} />
          </mesh>
          <MarsVehicleInfoBox vehicle={vehicle} />
        </>
      )}
    </group>
  )
}
