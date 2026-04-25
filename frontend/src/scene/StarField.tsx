import { useMemo, useRef } from 'react'
import * as THREE from 'three'

const STAR_COUNT = 5000
const SPHERE_RADIUS = 500

export function StarField() {
  const geoRef = useRef<THREE.BufferGeometry>(null)

  const positions = useMemo(() => {
    const arr = new Float32Array(STAR_COUNT * 3)
    let i = 0
    while (i < STAR_COUNT) {
      const x = (Math.random() - 0.5) * 2 * SPHERE_RADIUS
      const y = (Math.random() - 0.5) * 2 * SPHERE_RADIUS
      const z = (Math.random() - 0.5) * 2 * SPHERE_RADIUS
      if (x * x + y * y + z * z <= SPHERE_RADIUS * SPHERE_RADIUS) {
        arr[i * 3] = x
        arr[i * 3 + 1] = y
        arr[i * 3 + 2] = z
        i++
      }
    }
    return arr
  }, [])

  return (
    <points>
      <bufferGeometry ref={geoRef}>
        <bufferAttribute
          attach="attributes-position"
          array={positions}
          count={STAR_COUNT}
          itemSize={3}
        />
      </bufferGeometry>
      <pointsMaterial size={0.4} color="#ffffff" sizeAttenuation transparent opacity={0.85} />
    </points>
  )
}
