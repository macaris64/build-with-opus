import { useMemo } from 'react'
import * as THREE from 'three'

function buildMountainTerrain(): THREE.BufferGeometry {
  const geo = new THREE.PlaneGeometry(120, 120, 80, 80)
  const pos = geo.attributes.position as THREE.BufferAttribute
  for (let i = 0; i < pos.count; i++) {
    const x = pos.getX(i)
    const z = pos.getY(i) // plane is XY before rotation
    const ridge =
      Math.abs(Math.sin(x * 0.18) * Math.cos(z * 0.22)) * 4.0
      + Math.pow(Math.abs(Math.sin(x * 0.08 + z * 0.1)), 2) * 6.0
      + Math.sin(x * 0.4 + z * 0.3) * 0.8
      + Math.cos(x * 0.25 - z * 0.35) * 1.2
    pos.setZ(i, ridge)
  }
  pos.needsUpdate = true
  geo.computeVertexNormals()
  return geo
}

const LAKE_RIM_ROCKS: [number, number, number, number, number, number][] = [
  [-3.5,  0.06, -2.0, 1.2, 0.15, 0.8],
  [-10.5, 0.06, -3.5, 0.9, 0.12, 0.6],
  [-4.2,  0.06, -10.5, 1.4, 0.13, 0.9],
  [-11.0, 0.06, -8.5, 0.7, 0.11, 0.5],
  [-2.8,  0.06, -7.0, 1.0, 0.10, 0.7],
  [-12.0, 0.06, -6.0, 0.6, 0.09, 0.5],
]

export function TitanTerrain() {
  const mountainGeo = useMemo(() => buildMountainTerrain(), [])

  return (
    <group>
      {/* Mountainous rocky ground */}
      <mesh
        geometry={mountainGeo}
        rotation={[-Math.PI / 2, 0, 0]}
        position={[0, -0.01, 0]}
        receiveShadow
      >
        <meshStandardMaterial color="#7a4a1a" roughness={0.98} metalness={0.01} />
      </mesh>

      {/* Dark methane lake — near cryobot positions (negative X quadrant) */}
      {/* scale=[1.6,1,1] stretches the circle into an ellipse on the ground plane */}
      <mesh rotation={[-Math.PI / 2, 0, 0]} position={[-7, 0.02, -5]} scale={[1.6, 1, 1]} receiveShadow>
        <circleGeometry args={[5, 64]} />
        <meshStandardMaterial
          color="#1a0800"
          roughness={0.04}
          metalness={0.72}
          transparent
          opacity={0.93}
        />
      </mesh>

      {/* Lake rim rocks */}
      {LAKE_RIM_ROCKS.map(([x, y, z, w, h, d], i) => (
        <mesh key={i} position={[x, y, z]} receiveShadow>
          <boxGeometry args={[w, h, d]} />
          <meshStandardMaterial color="#5a3010" roughness={0.96} />
        </mesh>
      ))}
    </group>
  )
}
