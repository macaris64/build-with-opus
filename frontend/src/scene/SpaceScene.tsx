import { Suspense } from 'react'
import * as THREE from 'three'
import { Canvas } from '@react-three/fiber'
import { OrbitControls, Stars } from '@react-three/drei'
import { Mars } from './Mars'
import { OrbiterMesh } from './OrbiterMesh'
import { RoverNode } from './RoverNode'
import { GroundStationDish } from './GroundStationDish'
import { CommLink } from './CommLink'
import { useTelemetryStore } from '../store/telemetryStore'

function freshnessStatus(utc: string | null): 'active' | 'degraded' | 'dead' {
  if (!utc) return 'dead'
  const ageMs = Date.now() - new Date(utc).getTime()
  if (ageMs < 5000) return 'active'
  if (ageMs < 15000) return 'degraded'
  return 'dead'
}

// Orbiter nominal reference point for CommLink origins (actual position is animated in OrbiterMesh)
const ORBITER_NOM = new THREE.Vector3(0, 10, 0)

// Rovers are placed INSIDE Mars (sphere radius = 8). All positions have dist < 8 from origin.
const ROVER_LAND_POS = new THREE.Vector3(2, -5.0, 2.5)   // dist ≈ 5.8 — on Mars terrain
const ROVER_UAV_POS  = new THREE.Vector3(3, -3.5, 3)     // dist ≈ 5.5 — flying above terrain
const ROVER_CRYO_POS = new THREE.Vector3(-2.5, -5.0, -2) // dist ≈ 5.8 — on Mars terrain, drilling

// Ground station is OUTSIDE Mars, Earth-side
const GROUND_POS = new THREE.Vector3(-8, 2.2, 6)

function SceneContent() {
  const linksHealth = useTelemetryStore((s) => s.links_health)
  const link = useTelemetryStore((s) => s.link)

  const cfsRosStatus = freshnessStatus(linksHealth.find((l) => l.link === 'cfs-ros')?.last_hk_utc ?? null)
  const rosGndStatus = freshnessStatus(linksHealth.find((l) => l.link === 'ros-ground')?.last_hk_utc ?? null)
  const fleetStatus  = freshnessStatus(linksHealth.find((l) => l.link === 'fleet-dds')?.last_hk_utc ?? null)
  const aosStatus: 'active' | 'degraded' | 'dead' =
    link.state === 'Aos' ? 'active' : link.state === 'Degraded' ? 'degraded' : 'dead'

  return (
    <>
      <ambientLight intensity={0.25} />
      <directionalLight position={[20, 30, 10]} intensity={1.1} castShadow shadow-mapSize={[1024, 1024]} />
      {/* Warm Mars-bounce fill light from below */}
      <pointLight position={[0, -4, 0]} color="#c1440e" intensity={0.6} distance={20} />
      {/* Orbiter spotlight */}
      <pointLight position={[0, 12, 0]} color="#4488ff" intensity={0.4} distance={30} />

      {/* Mars planet — rovers live inside this sphere */}
      <Mars />

      {/* Orbiting spacecraft */}
      <OrbiterMesh />

      {/* Rovers — positioned on the Martian interior surface */}
      <RoverNode
        id="rover-land"
        position={[ROVER_LAND_POS.x, ROVER_LAND_POS.y, ROVER_LAND_POS.z]}
        label="Land Rover"
        hkApid={0x300}
        meshVariant="land"
      />
      <RoverNode
        id="rover-uav"
        position={[ROVER_UAV_POS.x, ROVER_UAV_POS.y, ROVER_UAV_POS.z]}
        label="Aerial Drone"
        hkApid={0x3c0}
        meshVariant="uav"
      />
      <RoverNode
        id="rover-cryobot"
        position={[ROVER_CRYO_POS.x, ROVER_CRYO_POS.y, ROVER_CRYO_POS.z]}
        label="Cryobot"
        hkApid={0x400}
        meshVariant="cryobot"
      />

      {/* Earth-side ground station (outside Mars) */}
      <GroundStationDish />

      {/* Comm link 1: cFS orbiter ↔ ROS rover fleet */}
      <CommLink
        from={ORBITER_NOM}
        to={ROVER_LAND_POS}
        status={cfsRosStatus}
        apidHex="0x128"
        label="cFS↔ROS"
      />
      {/* Comm link 2: DDS intra-rover fleet mesh */}
      <CommLink
        from={ROVER_LAND_POS}
        to={ROVER_CRYO_POS}
        status={fleetStatus}
        apidHex="0x160"
        label="DDS Fleet"
      />
      {/* Comm link 3: ROS UAV ↔ Ground (Proximity-1) */}
      <CommLink
        from={ROVER_UAV_POS}
        to={GROUND_POS}
        status={rosGndStatus}
        apidHex="0x129"
        label="ROS↔Gnd"
      />
      {/* Comm link 4: cFS orbiter ↔ Ground (AOS frame) */}
      <CommLink
        from={ORBITER_NOM}
        to={GROUND_POS}
        status={aosStatus}
        apidHex="AOS"
        label="cFS↔Gnd"
      />

      <OrbitControls
        enablePan={false}
        maxPolarAngle={Math.PI / 1.9}
        minDistance={5}
        maxDistance={70}
        target={[0, -1, 0]}
      />
    </>
  )
}

export function SpaceScene() {
  return (
    <Canvas
      camera={{ position: [0, 14, 28], fov: 54, near: 0.1, far: 2000 }}
      shadows
      gl={{ antialias: true }}
      onCreated={({ scene }) => {
        scene.background = new THREE.Color('#020408')
        scene.fog = new THREE.FogExp2('#020408', 0.006)
      }}
    >
      <Suspense fallback={null}>
        <Stars radius={300} depth={60} count={3000} factor={4} saturation={0} fade speed={0.4} />
        <SceneContent />
      </Suspense>
    </Canvas>
  )
}
