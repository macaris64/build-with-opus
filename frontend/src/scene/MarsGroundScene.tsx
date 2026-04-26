import { Suspense } from 'react'
import * as THREE from 'three'
import { Canvas } from '@react-three/fiber'
import { OrbitControls } from '@react-three/drei'
import { MarsSkyDome } from './MarsSkyDome'
import { MarsGroundTerrain } from './MarsGroundTerrain'
import { MarsAerialDrone } from './MarsAerialDrone'
import { MarsGroundVehicle } from './MarsGroundVehicle'
import { MarsCommArrows } from './MarsCommArrow'
import { useMarsStore } from '../store/marsStore'

// Per-drone animation parameters (deterministic)
const DRONE_PARAMS = [
  { hoverSpeed: 1.1, hoverPhase: 0.0, orbitRadius: 1.2, orbitSpeed: 0.17, orbitPhaseX: 0.0, orbitPhaseZ: 1.5 },
  { hoverSpeed: 0.9, hoverPhase: 1.2, orbitRadius: 1.5, orbitSpeed: 0.13, orbitPhaseX: 2.0, orbitPhaseZ: 0.5 },
  { hoverSpeed: 1.3, hoverPhase: 2.4, orbitRadius: 0.9, orbitSpeed: 0.21, orbitPhaseX: 1.1, orbitPhaseZ: 3.0 },
  { hoverSpeed: 0.8, hoverPhase: 0.7, orbitRadius: 1.8, orbitSpeed: 0.15, orbitPhaseX: 3.5, orbitPhaseZ: 1.8 },
  { hoverSpeed: 1.2, hoverPhase: 3.1, orbitRadius: 1.1, orbitSpeed: 0.19, orbitPhaseX: 0.8, orbitPhaseZ: 2.3 },
]

function SceneContent() {
  const vehicles = useMarsStore((s) => s.vehicles)
  const selectVehicle = useMarsStore((s) => s.selectVehicle)

  const drones = vehicles.filter((v) => v.type === 'drone')
  const groundVehicles = vehicles.filter((v) => v.type !== 'drone')

  return (
    <>
      {/* Lighting */}
      <ambientLight color="#7a5340" intensity={0.5} />
      <directionalLight
        position={[-20, 8, 10]}
        color="#e8a060"
        intensity={0.9}
        castShadow
        shadow-mapSize={[1024, 1024]}
      />
      {/* Water puddle cool fill */}
      <pointLight position={[-5, 0.5, -4]} color="#3a8ab0" intensity={0.5} distance={10} />
      {/* Warm ground bounce */}
      <pointLight position={[0, 1, 0]} color="#c27140" intensity={0.3} distance={30} />

      <MarsSkyDome />
      <MarsGroundTerrain />

      {/* Drones in the air */}
      {drones.map((vehicle, i) => (
        <MarsAerialDrone key={vehicle.id} vehicle={vehicle} {...DRONE_PARAMS[i]} />
      ))}

      {/* Ground vehicles */}
      {groundVehicles.map((vehicle) => (
        <MarsGroundVehicle key={vehicle.id} vehicle={vehicle} />
      ))}

      {/* Communication arrows */}
      <MarsCommArrows />

      {/* Camera controls — click on background to deselect */}
      <OrbitControls
        enablePan
        maxPolarAngle={Math.PI / 2.05}
        minDistance={4}
        maxDistance={60}
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

export function MarsGroundScene() {
  return (
    <Canvas
      camera={{ position: [0, 8, 22], fov: 60, near: 0.1, far: 500 }}
      shadows
      gl={{ antialias: true }}
      onCreated={({ scene }) => {
        scene.fog = new THREE.FogExp2('#c87941', 0.022)
        scene.background = new THREE.Color('#c27140')
      }}
    >
      <Suspense fallback={null}>
        <SceneContent />
      </Suspense>
    </Canvas>
  )
}
