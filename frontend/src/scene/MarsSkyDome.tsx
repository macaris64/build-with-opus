import { useMemo } from 'react'
import * as THREE from 'three'

const VERT = /* glsl */ `
  varying float vY;
  void main() {
    vY = normalize(position).y;
    gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
  }
`

const FRAG = /* glsl */ `
  uniform vec3 uHorizon;
  uniform vec3 uMid;
  uniform vec3 uZenith;
  varying float vY;
  void main() {
    float t = clamp(vY, 0.0, 1.0);
    vec3 col = mix(uHorizon, uMid, smoothstep(0.0, 0.35, t));
    col = mix(col, uZenith, smoothstep(0.35, 1.0, t));
    gl_FragColor = vec4(col, 1.0);
  }
`

export function MarsSkyDome() {
  const mat = useMemo(
    () =>
      new THREE.ShaderMaterial({
        vertexShader: VERT,
        fragmentShader: FRAG,
        uniforms: {
          uHorizon: { value: new THREE.Color('#e8a067') },
          uMid: { value: new THREE.Color('#b06040') },
          uZenith: { value: new THREE.Color('#5a3020') },
        },
        side: THREE.BackSide,
        depthWrite: false,
      }),
    [],
  )

  const geo = useMemo(() => new THREE.SphereGeometry(150, 32, 16), [])

  return <mesh geometry={geo} material={mat} />
}
