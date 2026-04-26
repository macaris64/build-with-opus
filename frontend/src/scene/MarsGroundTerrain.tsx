import { useMemo } from 'react'
import * as THREE from 'three'

function buildSandyGround(): THREE.BufferGeometry {
  const geo = new THREE.PlaneGeometry(120, 120, 60, 60)
  const pos = geo.attributes.position as THREE.BufferAttribute
  for (let i = 0; i < pos.count; i++) {
    const x = pos.getX(i)
    const z = pos.getY(i) // plane is XY before rotation, so Y maps to Z after rotation
    const dune =
      Math.sin(x * 0.35) * Math.cos(z * 0.28) * 0.55 +
      Math.sin(x * 0.8 + z * 0.6) * 0.2 +
      Math.cos(x * 0.5 - z * 0.9) * 0.15
    pos.setZ(i, dune)
  }
  pos.needsUpdate = true
  geo.computeVertexNormals()
  return geo
}

export function MarsGroundTerrain() {
  const sandGeo = useMemo(() => buildSandyGround(), [])

  return (
    <group>
      {/* Sandy ground */}
      <mesh
        geometry={sandGeo}
        rotation={[-Math.PI / 2, 0, 0]}
        position={[0, -0.01, 0]}
        receiveShadow
      >
        <meshStandardMaterial color="#c2874d" roughness={0.92} metalness={0.02} />
      </mesh>

      {/* Water puddle */}
      <mesh rotation={[-Math.PI / 2, 0, 0]} position={[-5, 0.05, -4]} receiveShadow>
        <circleGeometry args={[3, 48]} />
        <meshStandardMaterial
          color="#3a6b8a"
          roughness={0.08}
          metalness={0.55}
          transparent
          opacity={0.85}
        />
      </mesh>

      {/* Puddle rim rocks */}
      {([
        [-3.2, 0.06, -4.8, 0.9, 0.12, 0.6],
        [-6.5, 0.06, -3.5, 0.7, 0.1, 0.5],
        [-4.8, 0.06, -6.2, 1.1, 0.1, 0.7],
        [-6.2, 0.06, -5.2, 0.5, 0.09, 0.4],
      ] as [number, number, number, number, number, number][]).map(([x, y, z, w, h, d], i) => (
        <mesh key={i} position={[x, y, z]} rotation={[0, Math.random() * Math.PI, 0]} receiveShadow>
          <boxGeometry args={[w, h, d]} />
          <meshStandardMaterial color="#7a5030" roughness={0.95} />
        </mesh>
      ))}
    </group>
  )
}
