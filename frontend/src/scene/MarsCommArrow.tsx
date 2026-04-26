import { useMemo, useRef } from 'react'
import * as THREE from 'three'
import { useFrame } from '@react-three/fiber'
import { useMarsStore, VEHICLE_COLOR } from '../store/marsStore'

interface ArrowProps {
  from: THREE.Vector3
  to: THREE.Vector3
  color: string
  startTime: number
  expiresAt: number
}

function Arrow({ from, to, color, startTime, expiresAt }: ArrowProps) {
  const tubeMat = useRef<THREE.MeshBasicMaterial>(null)
  const headMat = useRef<THREE.MeshBasicMaterial>(null)
  const headRef = useRef<THREE.Mesh>(null)

  const dir = useMemo(() => to.clone().sub(from).normalize(), [from, to])
  const mid = useMemo(() => from.clone().add(to).multiplyScalar(0.5), [from, to])

  const tubePath = useMemo(
    () => new THREE.LineCurve3(from.clone(), to.clone().sub(dir.clone().multiplyScalar(0.5))),
    [from, to, dir],
  )
  const tubeGeo = useMemo(() => new THREE.TubeGeometry(tubePath, 1, 0.05, 6, false), [tubePath])

  // Orient arrowhead
  const headQuat = useMemo(() => {
    const q = new THREE.Quaternion()
    q.setFromUnitVectors(new THREE.Vector3(0, 1, 0), dir)
    return q
  }, [dir])

  useFrame(() => {
    const now = Date.now()
    const total = expiresAt - startTime
    const elapsed = now - startTime
    const progress = Math.max(0, Math.min(1, elapsed / total))
    // sin curve: fade in fast, hold, fade out
    const opacity = Math.sin(progress * Math.PI) * 0.9
    if (tubeMat.current) tubeMat.current.opacity = opacity
    if (headMat.current) headMat.current.opacity = opacity
  })

  const headPos = to.clone().sub(dir.clone().multiplyScalar(0.25))
  void mid // suppress unused

  return (
    <group>
      <mesh geometry={tubeGeo}>
        <meshBasicMaterial ref={tubeMat} color={color} transparent opacity={0.8} />
      </mesh>
      <mesh ref={headRef} position={[headPos.x, headPos.y, headPos.z]} quaternion={headQuat}>
        <coneGeometry args={[0.12, 0.35, 8]} />
        <meshBasicMaterial ref={headMat} color={color} transparent opacity={0.8} />
      </mesh>
      {/* Subtle glow line */}
      <line>
        <bufferGeometry>
          <bufferAttribute
            attach="attributes-position"
            args={[new Float32Array([from.x, from.y, from.z, to.x, to.y, to.z]), 3]}
          />
        </bufferGeometry>
        <lineBasicMaterial color={color} transparent opacity={0.3} />
      </line>
    </group>
  )
}

export function MarsCommArrows() {
  const vehicles = useMarsStore((s) => s.vehicles)
  const activeComms = useMarsStore((s) => s.activeComms)

  const vehiclePositions = useMemo(() => {
    const map = new Map<string, THREE.Vector3>()
    for (const v of vehicles) {
      map.set(v.id, new THREE.Vector3(...v.basePosition))
    }
    return map
  }, [vehicles])

  return (
    <>
      {activeComms.map((evt) => {
        const from = vehiclePositions.get(evt.fromId)
        const to = vehiclePositions.get(evt.toId)
        if (!from || !to) return null
        const vehicleType = vehicles.find((v) => v.id === evt.fromId)?.type ?? 'drone'
        const color = VEHICLE_COLOR[vehicleType]
        return (
          <Arrow
            key={evt.id}
            from={from}
            to={to}
            color={color}
            startTime={evt.timestamp}
            expiresAt={evt.expiresAt}
          />
        )
      })}
    </>
  )
}
