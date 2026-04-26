import { useRef } from 'react'
import * as THREE from 'three'
import { useFrame } from '@react-three/fiber'
import { useTitanStore, TITAN_VEHICLE_COLOR } from '../store/titanStore'
import { TitanVehicleInfoBox } from './TitanVehicleInfoBox'
import type { TitanVehicle } from '../types/titan'

function RoverMesh() {
  return (
    <>
      <mesh castShadow>
        <cylinderGeometry args={[0.5, 0.6, 0.4, 6]} />
        <meshStandardMaterial color="#3d1f00" roughness={0.8} metalness={0.15} />
      </mesh>
      {([[-0.5, -0.15, 0.3], [0.5, -0.15, 0.3], [-0.5, -0.15, -0.3], [0.5, -0.15, -0.3]] as [number, number, number][]).map(([x, y, z], i) => (
        <mesh key={i} position={[x, y, z]} castShadow>
          <boxGeometry args={[0.18, 0.18, 0.28]} />
          <meshStandardMaterial color="#2a1000" roughness={0.9} />
        </mesh>
      ))}
    </>
  )
}

function CryobotMesh() {
  return (
    <>
      <mesh castShadow>
        <cylinderGeometry args={[0.3, 0.3, 1.2, 12]} />
        <meshStandardMaterial color="#0e4a5a" roughness={0.3} metalness={0.65} />
      </mesh>
      <mesh position={[0, -0.8, 0]} castShadow>
        <coneGeometry args={[0.3, 0.4, 12]} />
        <meshStandardMaterial color="#073040" roughness={0.4} metalness={0.75} />
      </mesh>
    </>
  )
}

interface Props {
  vehicle: TitanVehicle
}

export function TitanGroundVehicle({ vehicle }: Props) {
  const groupRef = useRef<THREE.Group>(null)
  const pulseRef = useRef(0)
  const prevPacketRef = useRef<number | undefined>(undefined)

  const selectVehicle = useTitanStore((s) => s.selectVehicle)
  const selectedId = useTitanStore((s) => s.selectedVehicleId)
  const liveVehicle = useTitanStore((s) => s.vehicles.find((v) => v.id === vehicle.id) ?? vehicle)

  const isSelected = selectedId === vehicle.id
  const color = TITAN_VEHICLE_COLOR[vehicle.type]

  useFrame((_, delta) => {
    const g = groupRef.current
    if (!g) return
    if (liveVehicle.lastPacketAt !== prevPacketRef.current) {
      prevPacketRef.current = liveVehicle.lastPacketAt
      pulseRef.current = 1
    }
    if (pulseRef.current > 0) {
      pulseRef.current = Math.max(0, pulseRef.current - delta * 3)
      g.scale.setScalar(1 + pulseRef.current * 0.25)
    } else {
      g.scale.setScalar(1)
    }
  })

  const [x, y, z] = vehicle.basePosition

  return (
    <group
      ref={groupRef}
      position={[x, y, z]}
      onClick={(e) => { e.stopPropagation(); selectVehicle(isSelected ? null : vehicle.id) }}
    >
      {vehicle.type === 'rover'   && <RoverMesh />}
      {vehicle.type === 'cryobot' && <CryobotMesh />}

      {isSelected && (
        <>
          <mesh rotation={[Math.PI / 2, 0, 0]} position={[0, -0.3, 0]}>
            <torusGeometry args={[1.0, 0.04, 8, 64]} />
            <meshBasicMaterial color={color} transparent opacity={0.75} />
          </mesh>
          <mesh rotation={[Math.PI / 2, 0, 0]} position={[0, -0.3, 0]}>
            <torusGeometry args={[1.25, 0.02, 6, 64]} />
            <meshBasicMaterial color={color} transparent opacity={0.3} />
          </mesh>
          <TitanVehicleInfoBox vehicle={liveVehicle} />
        </>
      )}
    </group>
  )
}
