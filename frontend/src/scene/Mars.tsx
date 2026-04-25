import { useEffect, useRef } from 'react'
import * as THREE from 'three'
import { Html } from '@react-three/drei'

function MarsFloor() {
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
        Math.sin(x * 1.1) * Math.sin(z * 0.85) * 0.22 +
        Math.sin(x * 0.4 + 1.1) * Math.cos(z * 0.55 + 0.9) * 0.12 +
        Math.cos(x * 1.3 + z * 1.0) * 0.07
      pos.setY(i, y)
    }
    pos.needsUpdate = true
    geo.computeVertexNormals()
  }, [])

  return (
    <mesh ref={meshRef} position={[0, -5.5, 0]} rotation={[-Math.PI / 2, 0, 0]} receiveShadow>
      <circleGeometry args={[5.2, 128]} />
      <meshStandardMaterial color="#7a2c0c" roughness={0.95} metalness={0.02} />
    </mesh>
  )
}

export function Mars() {
  return (
    <group>
      {/* Atmospheric outer glow */}
      <mesh>
        <sphereGeometry args={[8.8, 32, 32]} />
        <meshBasicMaterial color="#ff4400" transparent opacity={0.025} depthWrite={false} />
      </mesh>

      {/* Outer shell — FrontSide, gives the planet's visible surface */}
      <mesh>
        <sphereGeometry args={[8, 64, 64]} />
        <meshStandardMaterial
          color="#a03010"
          transparent
          opacity={0.14}
          side={THREE.FrontSide}
          depthWrite={false}
          roughness={0.9}
          metalness={0.0}
        />
      </mesh>

      {/* Inner shell — BackSide, colours the interior dome when viewed from outside */}
      <mesh>
        <sphereGeometry args={[8, 64, 64]} />
        <meshStandardMaterial
          color="#c1440e"
          transparent
          opacity={0.22}
          side={THREE.BackSide}
          depthWrite={false}
          roughness={0.85}
          metalness={0.05}
        />
      </mesh>

      {/* Wireframe latitude/longitude grid */}
      <mesh>
        <sphereGeometry args={[8.06, 24, 24]} />
        <meshBasicMaterial color="#c1440e" transparent opacity={0.05} wireframe depthWrite={false} />
      </mesh>

      {/* Internal Martian terrain floor */}
      <MarsFloor />

      {/* Planet label */}
      <Html position={[0, 9.6, 0]} center>
        <div
          style={{
            fontFamily: '"JetBrains Mono", ui-monospace, monospace',
            fontSize: '13px',
            fontWeight: 700,
            color: '#ff6633',
            textShadow: '0 0 14px #ff440099, 0 0 32px #ff220055',
            letterSpacing: '5px',
            pointerEvents: 'none',
            userSelect: 'none',
            whiteSpace: 'nowrap',
          }}
        >
          MARS
        </div>
      </Html>
    </group>
  )
}
