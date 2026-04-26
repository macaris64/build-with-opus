import { Suspense } from 'react'
import * as THREE from 'three'
import { Canvas } from '@react-three/fiber'
import { OrbitControls } from '@react-three/drei'
import { TitanSkyDome } from './TitanSkyDome'
import { TitanTerrain } from './TitanTerrain'
import { TitanUAV } from './TitanUAV'
import { TitanGroundVehicle } from './TitanGroundVehicle'
import { TitanCommArrows } from './TitanCommArrow'
import { TitanSaturn } from './TitanSaturn'
import { useTitanStore } from '../store/titanStore'

// Per-UAV animation parameters (deterministic)
const UAV_PARAMS = [
  { hoverSpeed: 1.1, hoverPhase: 0.0, orbitRadius: 1.2, orbitSpeed: 0.17, orbitPhaseX: 0.0, orbitPhaseZ: 1.5 },
  { hoverSpeed: 0.9, hoverPhase: 1.2, orbitRadius: 1.5, orbitSpeed: 0.13, orbitPhaseX: 2.0, orbitPhaseZ: 0.5 },
  { hoverSpeed: 1.3, hoverPhase: 2.4, orbitRadius: 0.9, orbitSpeed: 0.21, orbitPhaseX: 1.1, orbitPhaseZ: 3.0 },
  { hoverSpeed: 0.8, hoverPhase: 0.7, orbitRadius: 1.8, orbitSpeed: 0.15, orbitPhaseX: 3.5, orbitPhaseZ: 1.8 },
  { hoverSpeed: 1.2, hoverPhase: 3.1, orbitRadius: 1.1, orbitSpeed: 0.19, orbitPhaseX: 0.8, orbitPhaseZ: 2.3 },
]

function SceneContent() {
  const vehicles = useTitanStore((s) => s.vehicles)
  const selectVehicle = useTitanStore((s) => s.selectVehicle)

  const uavs = vehicles.filter((v) => v.type === 'uav')
  const groundVehicles = vehicles.filter((v) => v.type !== 'uav')

  return (
    <>
      {/* Lighting — warm amber Titan sun, cool methane lake fill */}
      <ambientLight color="#4a2800" intensity={0.4} />
      <directionalLight
        position={[-15, 6, 10]}
        color="#c87830"
        intensity={0.7}
        castShadow
        shadow-mapSize={[1024, 1024]}
      />
      {/* Lake cool reflection fill */}
      <pointLight position={[-7, 0.5, -5]} color="#0a3040" intensity={0.6} distance={14} />
      {/* Warm ground bounce */}
      <pointLight position={[0, 1, 0]} color="#8a4010" intensity={0.25} distance={35} />

      <TitanSkyDome />
      <TitanSaturn />
      <TitanTerrain />

      {/* UAVs in the sky */}
      {uavs.map((vehicle, i) => (
        <TitanUAV key={vehicle.id} vehicle={vehicle} {...UAV_PARAMS[i]} />
      ))}

      {/* Ground vehicles (rovers + cryobots) */}
      {groundVehicles.map((vehicle) => (
        <TitanGroundVehicle key={vehicle.id} vehicle={vehicle} />
      ))}

      {/* Communication arrows */}
      <TitanCommArrows />

      <OrbitControls
        enablePan
        maxPolarAngle={Math.PI / 2.05}
        minDistance={4}
        maxDistance={70}
        target={[0, 2, 0]}
      />

      {/* Deselect on background click */}
      <mesh
        position={[0, -0.05, 0]}
        rotation={[-Math.PI / 2, 0, 0]}
        visible={false}
        onClick={() => selectVehicle(null)}
      >
        <planeGeometry args={[200, 200]} />
        <meshBasicMaterial />
      </mesh>
    </>
  )
}

export function TitanGroundScene() {
  return (
    <Canvas
      camera={{ position: [0, 10, 25], fov: 60, near: 0.1, far: 500 }}
      shadows
      gl={{ antialias: true }}
      onCreated={({ scene }) => {
        scene.fog = new THREE.FogExp2('#8a4010', 0.018)
        scene.background = new THREE.Color('#6b3010')
      }}
    >
      <Suspense fallback={null}>
        <SceneContent />
      </Suspense>
    </Canvas>
  )
}
