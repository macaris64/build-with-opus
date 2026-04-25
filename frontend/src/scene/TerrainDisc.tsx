import { useEffect, useRef } from 'react'
import * as THREE from 'three'

export function TerrainDisc() {
  const meshRef = useRef<THREE.Mesh>(null)

  useEffect(() => {
    const mesh = meshRef.current
    if (!mesh) return
    const geo = mesh.geometry as THREE.BufferGeometry
    const pos = geo.attributes['position'] as THREE.BufferAttribute
    for (let i = 0; i < pos.count; i++) {
      const x = pos.getX(i)
      const z = pos.getZ(i)
      const y =
        Math.sin(x * 0.8) * Math.sin(z * 0.6) * 0.3 +
        Math.sin(x * 0.3 + 1.0) * Math.cos(z * 0.4 + 0.5) * 0.15 +
        Math.cos(x * 1.2 + z * 0.9) * 0.08
      pos.setY(i, y)
    }
    pos.needsUpdate = true
    geo.computeVertexNormals()
  }, [])

  return (
    <mesh ref={meshRef} rotation={[-Math.PI / 2, 0, 0]} receiveShadow>
      <circleGeometry args={[18, 128]} />
      <meshStandardMaterial color="#2d3e2a" roughness={0.92} metalness={0.05} />
    </mesh>
  )
}
