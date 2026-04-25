import { useRef, useMemo } from 'react'
import * as THREE from 'three'
import { useFrame } from '@react-three/fiber'

interface Props {
  curve: THREE.CatmullRomCurve3
  color: string
  active: boolean
  speed?: number
}

const PARTICLE_COUNT = 40

export function LinkParticles({ curve, color, active, speed = 0.5 }: Props) {
  const pointsRef = useRef<THREE.Points>(null)
  const geoRef = useRef<THREE.BufferGeometry>(null)

  const tValues = useMemo(() => {
    const arr = new Float32Array(PARTICLE_COUNT)
    for (let i = 0; i < PARTICLE_COUNT; i++) arr[i] = i / PARTICLE_COUNT
    return arr
  }, [])

  const positions = useMemo(() => new Float32Array(PARTICLE_COUNT * 3), [])

  useFrame((_, delta) => {
    if (!active) return
    const geo = geoRef.current
    if (!geo) return

    for (let i = 0; i < PARTICLE_COUNT; i++) {
      tValues[i] = (tValues[i]! + delta * speed) % 1
      const pt = curve.getPoint(tValues[i]!)
      positions[i * 3] = pt.x
      positions[i * 3 + 1] = pt.y
      positions[i * 3 + 2] = pt.z
    }

    const attr = geo.attributes['position'] as THREE.BufferAttribute
    attr.set(positions)
    attr.needsUpdate = true
  })

  return (
    <points ref={pointsRef}>
      <bufferGeometry ref={geoRef}>
        <bufferAttribute
          attach="attributes-position"
          array={positions}
          count={PARTICLE_COUNT}
          itemSize={3}
        />
      </bufferGeometry>
      <pointsMaterial
        size={0.18}
        color={color}
        sizeAttenuation
        transparent
        opacity={active ? 0.9 : 0}
      />
    </points>
  )
}
