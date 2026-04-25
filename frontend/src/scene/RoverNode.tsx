import { useEffect, useRef } from 'react'
import * as THREE from 'three'
import { useFrame } from '@react-three/fiber'
import { useTelemetryStore } from '../store/telemetryStore'
import { useUiStore } from '../store/uiStore'
import { VehicleInfoBox } from './VehicleInfoBox'

interface Props {
  id: string
  position: [number, number, number]
  label: string
  hkApid: number
  meshVariant: 'land' | 'uav' | 'cryobot'
}

function LandMesh() {
  return (
    <>
      <mesh castShadow>
        <cylinderGeometry args={[0.5, 0.6, 0.4, 6]} />
        <meshStandardMaterial color="#886633" roughness={0.7} metalness={0.2} />
      </mesh>
      {[[-0.5, -0.15, 0.3], [0.5, -0.15, 0.3], [-0.5, -0.15, -0.3], [0.5, -0.15, -0.3]].map(([x, y, z], i) => (
        <mesh key={i} position={[x as number, y as number, z as number]} castShadow>
          <boxGeometry args={[0.18, 0.18, 0.28]} />
          <meshStandardMaterial color="#553322" roughness={0.9} />
        </mesh>
      ))}
    </>
  )
}

function UavMesh() {
  return (
    <>
      <mesh castShadow>
        <coneGeometry args={[0.3, 0.6, 4]} />
        <meshStandardMaterial color="#667799" roughness={0.4} metalness={0.5} />
      </mesh>
      {[[0.5, 0.1, 0.5], [-0.5, 0.1, 0.5], [0.5, 0.1, -0.5], [-0.5, 0.1, -0.5]].map(([x, y, z], i) => (
        <mesh key={i} position={[x as number, y as number, z as number]} rotation={[Math.PI / 2, 0, 0]} castShadow>
          <cylinderGeometry args={[0.18, 0.18, 0.04, 8]} />
          <meshStandardMaterial color="#334455" roughness={0.3} metalness={0.6} />
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
        <meshStandardMaterial color="#557799" roughness={0.3} metalness={0.6} />
      </mesh>
      <mesh position={[0, -0.8, 0]} castShadow>
        <coneGeometry args={[0.3, 0.4, 12]} />
        <meshStandardMaterial color="#334466" roughness={0.4} metalness={0.7} />
      </mesh>
    </>
  )
}

const VARIANT_RING_COLOR: Record<string, string> = {
  land: '#f97316',
  uav: '#38bdf8',
  cryobot: '#a78bfa',
}

export function RoverNode({ id, position, label, hkApid, meshVariant }: Props) {
  const groupRef = useRef<THREE.Group>(null)
  const prevTimestampRef = useRef<string | null>(null)
  const pulseRef = useRef(0)
  const selectNode = useUiStore((s) => s.selectNode)
  const selectedId = useUiStore((s) => s.selectedNodeId)
  const hk = useTelemetryStore((s) => s.hk)

  const snapshot = hk.find((s) => s.apid === hkApid)
  const latestTs = snapshot?.frames[snapshot.frames.length - 1]?.timestamp_utc ?? null

  useEffect(() => {
    if (latestTs && latestTs !== prevTimestampRef.current) {
      prevTimestampRef.current = latestTs
      pulseRef.current = 1
    }
  }, [latestTs])

  useFrame((_, delta) => {
    const group = groupRef.current
    if (!group) return
    if (pulseRef.current > 0) {
      pulseRef.current = Math.max(0, pulseRef.current - delta * 3)
      const s = 1 + pulseRef.current * 0.35
      group.scale.setScalar(s)
    } else {
      group.scale.setScalar(1)
    }
  })

  const isSelected = selectedId === id
  const ringColor = VARIANT_RING_COLOR[meshVariant] ?? '#00ff88'

  return (
    <group
      ref={groupRef}
      position={position}
      onClick={(e) => { e.stopPropagation(); selectNode(isSelected ? null : id) }}
    >
      {meshVariant === 'land' && <LandMesh />}
      {meshVariant === 'uav' && <UavMesh />}
      {meshVariant === 'cryobot' && <CryobotMesh />}

      {isSelected && (
        <>
          {/* Glowing selection ring beneath vehicle */}
          <mesh rotation={[Math.PI / 2, 0, 0]} position={[0, -0.3, 0]}>
            <torusGeometry args={[1.0, 0.04, 8, 64]} />
            <meshBasicMaterial color={ringColor} transparent opacity={0.75} />
          </mesh>
          {/* Outer pulse ring */}
          <mesh rotation={[Math.PI / 2, 0, 0]} position={[0, -0.3, 0]}>
            <torusGeometry args={[1.25, 0.02, 6, 64]} />
            <meshBasicMaterial color={ringColor} transparent opacity={0.3} />
          </mesh>
          {/* 3D info panel */}
          <VehicleInfoBox label={label} apid={hkApid} variant={meshVariant} />
        </>
      )}
    </group>
  )
}
