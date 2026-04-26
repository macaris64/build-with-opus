import { useMemo } from 'react'
import * as THREE from 'three'

// ─── Saturn body shaders ──────────────────────────────────────────────────────

const SATURN_BODY_VERT = /* glsl */ `
  varying vec3 vNorm;
  void main() {
    vNorm = normalize(position);
    gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
  }
`

const SATURN_BODY_FRAG = /* glsl */ `
  varying vec3 vNorm;
  void main() {
    float lat    = vNorm.y;
    float absLat = abs(lat);

    vec3 cEqZone = vec3(0.933, 0.878, 0.706); // bright cream
    vec3 cEqBelt = vec3(0.753, 0.471, 0.188); // amber-brown belt
    vec3 cTempZ  = vec3(0.831, 0.722, 0.471); // warm tan
    vec3 cSubPol = vec3(0.847, 0.784, 0.533); // pale tan
    vec3 cPolCap = vec3(0.722, 0.627, 0.408); // muted amber-gray

    vec3 col = cEqZone;
    col = mix(col, cEqBelt, smoothstep(0.06, 0.16, absLat));
    col = mix(col, cTempZ,  smoothstep(0.18, 0.30, absLat));
    col = mix(col, cSubPol, smoothstep(0.44, 0.56, absLat));
    col = mix(col, cPolCap, smoothstep(0.62, 0.75, absLat));

    // Sub-band striping
    float fineBand = sin(lat * 28.0) * 0.055 + sin(lat * 11.0) * 0.04;
    col += vec3(fineBand * 0.6, fineBand * 0.45, fineBand * 0.2);
    col  = clamp(col, 0.0, 1.0);

    // Lambertian shading — matches scene directional light at [-15, 6, 10]
    vec3  lightDir = normalize(vec3(-0.789, 0.316, 0.526));
    float diff     = clamp(dot(vNorm, lightDir), 0.0, 1.0) * 0.55 + 0.45;
    col *= diff;

    gl_FragColor = vec4(col, 1.0);
  }
`

// ─── Ring shaders ─────────────────────────────────────────────────────────────

const SATURN_RING_VERT = /* glsl */ `
  varying float vRadius;
  void main() {
    // RingGeometry guarantees position.z == 0; radius is length of XY component
    vRadius     = length(position.xy);
    gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
  }
`

const SATURN_RING_FRAG = /* glsl */ `
  uniform float uInnerR;
  uniform float uOuterR;
  uniform float uCassiniIn;
  uniform float uCassiniOut;
  varying float vRadius;

  void main() {
    if (vRadius < uInnerR || vRadius > uOuterR) discard;

    // B-ring: inner bright ring
    float inB   = step(uInnerR, vRadius)   * step(vRadius, uCassiniIn);
    vec3  bCol  = vec3(0.910, 0.878, 0.800);
    float bBand = sin((vRadius - uInnerR) * 2.8) * 0.06;
    bCol       += vec3(bBand * 0.5, bBand * 0.4, bBand * 0.2);
    float bEdge = smoothstep(uInnerR, uInnerR + 0.8, vRadius);
    float bA    = 0.92 * bEdge;

    // Cassini Division: dark gap
    float inCas  = step(uCassiniIn,  vRadius) * step(vRadius, uCassiniOut);
    float casMsk = smoothstep(uCassiniIn,  uCassiniIn  + 0.4, vRadius)
                 * smoothstep(uCassiniOut, uCassiniOut - 0.4, vRadius);
    float casA   = 0.06 * casMsk;
    vec3  casCol = vec3(0.08, 0.04, 0.02);

    // A-ring: outer semi-transparent ring
    float inA   = step(uCassiniOut, vRadius) * step(vRadius, uOuterR);
    vec3  aCol  = vec3(0.831, 0.753, 0.565);
    float aBand = sin((vRadius - uCassiniOut) * 2.0) * 0.05;
    aCol       += vec3(aBand * 0.5, aBand * 0.35, aBand * 0.1);
    float aEdge = smoothstep(uOuterR, uOuterR - 1.2, vRadius);
    float aA    = 0.78 * aEdge;

    vec3  col   = bCol  * inB + casCol * inCas + aCol  * inA;
    float alpha = bA    * inB + casA   * inCas + aA    * inA;

    if (alpha < 0.01) discard;
    gl_FragColor = vec4(col, alpha);
  }
`

// ─── Glow shaders ─────────────────────────────────────────────────────────────

const SATURN_GLOW_VERT = /* glsl */ `
  varying vec3 vNorm;
  varying vec3 vViewPosition;
  void main() {
    vNorm          = normalize(normalMatrix * normal);
    vec4 mvPos     = modelViewMatrix * vec4(position, 1.0);
    vViewPosition  = -mvPos.xyz;
    gl_Position    = projectionMatrix * mvPos;
  }
`

const SATURN_GLOW_FRAG = /* glsl */ `
  varying vec3 vNorm;
  varying vec3 vViewPosition;
  void main() {
    vec3  viewDir   = normalize(vViewPosition);
    float rim       = 1.0 - abs(dot(vNorm, viewDir));
    rim             = pow(rim, 1.8);
    gl_FragColor    = vec4(vec3(0.95, 0.78, 0.35), rim * 0.18);
  }
`

// ─── Component ────────────────────────────────────────────────────────────────

// From Titan, Saturn subtends ~5.7° — placed at [-35, 65, -75] (~105 units from
// origin) with body radius 15 to appear as a dominant presence in the sky.
// Ring tilt: x=1.0996 rad (63°) gives the 27° from edge-on matching Saturn's
// 26.7° axial tilt as seen from Titan's orbital plane.
export function TitanSaturn() {
  const bodyMat = useMemo(
    () =>
      new THREE.ShaderMaterial({
        vertexShader:   SATURN_BODY_VERT,
        fragmentShader: SATURN_BODY_FRAG,
      }),
    [],
  )

  const ringMat = useMemo(
    () =>
      new THREE.ShaderMaterial({
        vertexShader:   SATURN_RING_VERT,
        fragmentShader: SATURN_RING_FRAG,
        uniforms: {
          uInnerR:    { value: 21.0 },
          uOuterR:    { value: 34.0 },
          uCassiniIn: { value: 25.5 },
          uCassiniOut:{ value: 27.0 },
        },
        transparent: true,
        side:        THREE.DoubleSide,
        depthWrite:  false,
      }),
    [],
  )

  const glowMat = useMemo(
    () =>
      new THREE.ShaderMaterial({
        vertexShader:   SATURN_GLOW_VERT,
        fragmentShader: SATURN_GLOW_FRAG,
        transparent:    true,
        side:           THREE.FrontSide,
        depthWrite:     false,
      }),
    [],
  )

  const bodyGeo = useMemo(() => new THREE.SphereGeometry(15, 64, 32), [])
  const ringGeo = useMemo(() => new THREE.RingGeometry(21, 34, 128, 4), [])
  const glowGeo = useMemo(() => new THREE.SphereGeometry(16.5, 32, 16), [])

  return (
    <group position={[-35, 65, -75]}>
      {/* Glow renders first — depthWrite false so it doesn't occlude body/rings */}
      <mesh geometry={glowGeo} material={glowMat} />

      {/* Planet body — opaque, writes depth */}
      <mesh geometry={bodyGeo} material={bodyMat} />

      {/* Rings — DoubleSide so both faces are visible as camera orbits */}
      <mesh geometry={ringGeo} material={ringMat} rotation={[1.0996, 0, 0.2]} />
    </group>
  )
}
