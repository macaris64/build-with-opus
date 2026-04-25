import { useMemo } from 'react'
import * as THREE from 'three'
import { LinkParticles } from './LinkParticles'
import { NodeLabel } from './NodeLabel'

type LinkStatus = 'active' | 'degraded' | 'dead'

interface Props {
  from: THREE.Vector3
  to: THREE.Vector3
  status: LinkStatus
  apidHex: string
  label: string
}

const STATUS_COLORS: Record<LinkStatus, string> = {
  active: '#00ff88',
  degraded: '#ffaa00',
  dead: '#ff2244',
}

export function CommLink({ from, to, status, apidHex, label }: Props) {
  const curve = useMemo(() => {
    const mid = new THREE.Vector3().addVectors(from, to).multiplyScalar(0.5)
    mid.y += 3
    return new THREE.CatmullRomCurve3([from, mid, to])
  }, [from, to])

  const tubeGeo = useMemo(
    () => new THREE.TubeGeometry(curve, 32, 0.04, 6, false),
    [curve],
  )

  const color = STATUS_COLORS[status]
  const midPt = curve.getPoint(0.5)
  const labelPos: [number, number, number] = [midPt.x, midPt.y + 0.5, midPt.z]

  return (
    <group>
      <mesh>
        <primitive object={tubeGeo} />
        <meshStandardMaterial
          color={color}
          transparent
          opacity={status === 'dead' ? 0.1 : 0.55}
          emissive={color}
          emissiveIntensity={status === 'dead' ? 0 : 0.3}
        />
      </mesh>
      <LinkParticles
        curve={curve}
        color={color}
        active={status !== 'dead'}
        speed={status === 'degraded' ? 0.2 : 0.5}
      />
      <NodeLabel position={labelPos} label={label} apidHex={apidHex} color={color} />
    </group>
  )
}
