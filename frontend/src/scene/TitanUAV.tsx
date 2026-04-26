import { useRef } from 'react'
import * as THREE from 'three'
import { useFrame } from '@react-three/fiber'
import { useTitanStore, TITAN_VEHICLE_COLOR } from '../store/titanStore'
import { TitanVehicleInfoBox } from './TitanVehicleInfoBox'
import type { TitanVehicle } from '../types/titan'

interface Props {
  vehicle: TitanVehicle
  hoverSpeed: number
  hoverPhase: number
  orbitRadius: number
  orbitSpeed: number
  orbitPhaseX: number
  orbitPhaseZ: number
}

export function TitanUAV({ vehicle, hoverSpeed, hoverPhase, orbitRadius, orbitSpeed, orbitPhaseX, orbitPhaseZ }: Props) {
  const groupRef = useRef<THREE.Group>(null)
  const rotorRef = useRef<THREE.Group>(null)
  const selectVehicle = useTitanStore((s) => s.selectVehicle)
  const selectedId = useTitanStore((s) => s.selectedVehicleId)
  const isSelected = selectedId === vehicle.id
  const color = TITAN_VEHICLE_COLOR.uav

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
        <coneGeometry args={[0.35, 0.7, 4]} />
        <meshStandardMaterial color="#8a6633" roughness={0.4} metalness={0.5} />
      </mesh>

      {/* Rotor group — larger blades for Titan's thick atmosphere */}
      <group ref={rotorRef}>
        {([[0.6, 0.14, 0.6], [-0.6, 0.14, 0.6], [0.6, 0.14, -0.6], [-0.6, 0.14, -0.6]] as [number, number, number][]).map(([x, y, z], i) => (
          <mesh key={i} position={[x, y, z]} rotation={[Math.PI / 2, 0, 0]} castShadow>
            <cylinderGeometry args={[0.28, 0.28, 0.04, 8]} />
            <meshStandardMaterial color={color} roughness={0.2} metalness={0.7} emissive={color} emissiveIntensity={0.15} />
          </mesh>
        ))}
      </group>

      {/* Rotor arm spars */}
      {([[0.6, 0.14, 0.6], [-0.6, 0.14, 0.6], [0.6, 0.14, -0.6], [-0.6, 0.14, -0.6]] as [number, number, number][]).map(([x, y, z], i) => (
        <mesh key={`arm-${i}`} position={[x / 2, y, z / 2]}>
          <boxGeometry args={[Math.abs(x), 0.03, 0.03]} />
          <meshStandardMaterial color="#554422" roughness={0.5} />
        </mesh>
      ))}

      {isSelected && (
        <>
          <mesh rotation={[Math.PI / 2, 0, 0]} position={[0, -0.4, 0]}>
            <torusGeometry args={[1.1, 0.04, 8, 64]} />
            <meshBasicMaterial color={color} transparent opacity={0.75} />
          </mesh>
          <mesh rotation={[Math.PI / 2, 0, 0]} position={[0, -0.4, 0]}>
            <torusGeometry args={[1.4, 0.02, 6, 64]} />
            <meshBasicMaterial color={color} transparent opacity={0.3} />
          </mesh>
          <TitanVehicleInfoBox vehicle={vehicle} />
        </>
      )}
    </group>
  )
}
