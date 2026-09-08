import * as THREE from 'three'

// Models are procedural and self-contained: the scene JSON is enough to rebuild them offline.
export function buildIndustrialModel(node) {
  const root = new THREE.Group()
  const metal = new THREE.MeshStandardMaterial({ color: '#c2cdd4', metalness: 0.72, roughness: 0.32 })
  const body = new THREE.MeshStandardMaterial({ color: node.color, metalness: 0.46, roughness: 0.38 })
  const dark = new THREE.MeshStandardMaterial({ color: '#354758', metalness: 0.5, roughness: 0.5 })
  const concrete = new THREE.MeshStandardMaterial({ color: '#b9c3ca', roughness: 0.88 })
  const yellow = new THREE.MeshStandardMaterial({ color: '#e2af53', roughness: 0.6, metalness: 0.3 })
  const materials = [metal, body, dark, concrete, yellow]
  const add = (geometry, material, position = [0, 0, 0], rotation = [0, 0, 0], parent = root) => {
    const mesh = new THREE.Mesh(geometry, material)
    mesh.position.set(...position)
    mesh.rotation.set(...rotation)
    mesh.castShadow = true
    mesh.receiveShadow = true
    parent.add(mesh)
    return mesh
  }
  const box = (size, material, pos, parent = root) => add(new THREE.BoxGeometry(...size), material, pos, undefined, parent)
  const cylinder = (r, length, material, pos, axis = 'y', parent = root) => add(
    new THREE.CylinderGeometry(r, r, length, 28), material, pos,
    axis === 'x' ? [0, 0, Math.PI / 2] : axis === 'z' ? [Math.PI / 2, 0, 0] : [0, 0, 0], parent)
  const ring = (r, tube, pos, axis = 'y', material = metal, parent = root) => add(
    new THREE.TorusGeometry(r, tube, 8, 36), material, pos,
    axis === 'y' ? [Math.PI / 2, 0, 0] : axis === 'x' ? [0, Math.PI / 2, 0] : [0, 0, 0], parent)
  const flange = (pos, axis = 'x') => cylinder(0.32, 0.13, metal, pos, axis)
  let rotor = null, liquid = null, indicator = null, height = 2

  function motor(x = 0, y = 0.75, z = 0) {
    cylinder(0.43, 1.45, body, [x, y, z], 'x')
    cylinder(0.47, 0.12, dark, [x + 0.74, y, z], 'x')
    for (let i = 0; i < 9; i++) {
      const a = i * Math.PI * 2 / 9
      const fin = box([1.17, 0.08, 0.16], body, [x, y + Math.cos(a) * 0.43, z + Math.sin(a) * 0.43])
      fin.rotation.x = a
    }
    box([0.55, 0.26, 0.48], dark, [x + 0.2, y + 0.49, z])
    box([1.1, 0.15, 0.95], dark, [x, 0.18, z])
  }

  if (node.type === 'tank') {
    height = 5.5
    for (const x of [-0.85, 0.85]) for (const z of [-0.85, 0.85]) {
      cylinder(0.1, 0.6, dark, [x, 0.3, z])
      box([0.4, 0.08, 0.4], metal, [x, 0.04, z])
    }
    const glass = new THREE.MeshStandardMaterial({ color: node.color, transparent: true, opacity: 0.27, metalness: 0.28, roughness: 0.2, depthWrite: false, side: THREE.DoubleSide })
    materials.push(glass)
    cylinder(1.3, 3.6, glass, [0, 2.5, 0])
    for (const y of [0.7, 1.0, 2.5, 4.05, 4.3]) ring(1.3, 0.045, [0, y, 0])
    cylinder(1.3, 0.14, metal, [0, 0.7, 0])
    add(new THREE.SphereGeometry(1.3, 32, 16, 0, Math.PI * 2, 0, Math.PI / 2), metal, [0, 4.3, 0]).scale.y = 0.32
    cylinder(0.3, 0.32, metal, [0, 4.85, 0])
    cylinder(0.43, 0.1, dark, [0, 5.04, 0])
    const water = new THREE.MeshStandardMaterial({ color: '#38a9c3', metalness: 0.24, roughness: 0.17, transparent: true, opacity: 0.8 })
    materials.push(water)
    liquid = cylinder(1.22, 1, water, [0, 0.78, 0])
    liquid.visible = false
    cylinder(0.18, 1, metal, [1.6, 1.1, 0], 'x')
    flange([2.1, 1.1, 0])
    for (const x of [-0.26, 0.26]) cylinder(0.035, 4.2, dark, [x, 2.7, 1.4])
    for (let y = 0.8; y < 4.8; y += 0.35) cylinder(0.025, 0.52, metal, [0, y, 1.4], 'x')
  } else if (node.type === 'pump') {
    height = 2.25
    box([3.3, 0.22, 1.6], concrete, [0, 0.11, 0])
    box([2.9, 0.14, 1.22], dark, [0, 0.3, 0])
    motor(0.6, 0.95)
    cylinder(0.64, 0.5, body, [-0.8, 1.03, 0], 'z')
    cylinder(0.43, 0.07, metal, [-0.8, 1.03, 0.29], 'z')
    rotor = new THREE.Group()
    rotor.position.set(-0.8, 1.03, 0.34)
    root.add(rotor)
    for (let i = 0; i < 3; i++) {
      const blade = box([0.58, 0.09, 0.035], dark, [0, 0, 0], rotor)
      blade.rotation.z = i * Math.PI / 3
    }
    cylinder(0.19, 0.95, body, [-0.8, 1.03, -0.7], 'z')
    flange([-0.8, 1.03, -1.18], 'z')
    cylinder(0.18, 0.6, body, [-0.8, 1.58, 0])
    flange([-0.8, 1.91, 0], 'y')
  } else if (node.type === 'motor') {
    height = 1.9
    box([2.1, 0.2, 1.5], concrete, [0, 0.1, 0])
    motor()
    rotor = cylinder(0.12, 0.55, metal, [-0.97, 0.75, 0], 'x')
    rotor.userData.axis = 'x'
  } else if (node.type === 'valve') {
    height = 2.4
    cylinder(0.2, 1.8, metal, [0, 1.1, 0], 'x')
    flange([-0.85, 1.1, 0]); flange([0.85, 1.1, 0])
    add(new THREE.SphereGeometry(0.42, 24, 16), body, [0, 1.1, 0])
    cylinder(0.08, 0.7, metal, [0, 1.65, 0])
    rotor = new THREE.Group(); rotor.position.y = 2.05; rotor.userData.axis = 'y'; root.add(rotor)
    ring(0.38, 0.045, [0, 0, 0], 'y', body, rotor)
    box([0.75, 0.04, 0.04], body, [0, 0, 0], rotor)
    box([0.04, 0.04, 0.75], body, [0, 0, 0], rotor)
    box([0.65, 0.18, 0.65], concrete, [0, 0.09, 0])
    cylinder(0.06, 0.8, dark, [0, 0.55, 0])
  } else if (node.type === 'fan') {
    height = 3.25
    box([2.4, 0.2, 1.3], concrete, [0, 0.1, 0])
    for (const x of [-0.75, 0.75]) box([0.12, 1.55, 0.55], dark, [x, 0.9, 0])
    ring(1.03, 0.17, [0, 1.75, 0], 'z', body)
    ring(1.03, 0.05, [0, 1.75, 0.38], 'z')
    cylinder(0.23, 0.45, metal, [0, 1.75, 0], 'z')
    rotor = new THREE.Group(); rotor.position.set(0, 1.75, 0.15); root.add(rotor)
    for (let i = 0; i < 5; i++) {
      const a = i * Math.PI * 2 / 5
      const blade = box([0.32, 0.74, 0.055], body, [Math.sin(a) * 0.5, Math.cos(a) * 0.5, 0], rotor)
      blade.rotation.z = -a + 0.3
    }
  } else if (node.type === 'pipe') {
    height = 1.65
    const length = node.pipeLength
    cylinder(0.18, length, body, [0, 1.1, 0], 'x')
    flange([-length / 2, 1.1, 0]); flange([length / 2, 1.1, 0])
    for (const x of [-length * 0.32, length * 0.32]) {
      box([0.12, 0.9, 0.12], metal, [x, 0.48, 0])
      box([0.55, 0.12, 0.5], concrete, [x, 0.06, 0])
    }
    rotor = new THREE.Group(); root.add(rotor)
    for (let i = 0; i < 3; i++) {
      const arrow = add(new THREE.ConeGeometry(0.09, 0.23, 8), yellow, [(i - 1) * length / 3, 1.37, 0], [0, 0, -Math.PI / 2], rotor)
      arrow.userData.startX = arrow.position.x
    }
  } else if (node.type === 'building') {
    height = 5.6
    for (const x of [-4, 4]) for (const z of [-3, 0, 3]) box([0.18, 5, 0.18], body, [x, 2.5, z])
    for (const z of [-3, 0, 3]) box([8.2, 0.2, 0.18], metal, [0, 5, z])
    for (const x of [-4, 4]) box([0.2, 0.2, 6.2], metal, [x, 5, 0])
    box([8.5, 0.15, 6.5], body, [0, 5.2, 0])
  } else if (node.type === 'platform') {
    height = 0.4
    box([6, 0.25, 5], body, [0, 0.125, 0])
    for (const z of [-2.46, 2.46]) box([5.9, 0.015, 0.08], yellow, [0, 0.26, z])
    for (const x of [-2.96, 2.96]) box([0.08, 0.015, 4.9], yellow, [x, 0.26, 0])
  }
  if (!['building', 'platform', 'pipe'].includes(node.type)) {
    const lamp = new THREE.MeshStandardMaterial({ color: '#94a3b8', emissive: '#94a3b8', emissiveIntensity: 0.18 })
    materials.push(lamp)
    indicator = add(new THREE.SphereGeometry(0.09, 12, 8), lamp, [0.45, height - 0.45, 0.55])
  }
  root.userData = { nodeId: node.id, rotor, liquid, indicator, height, materials }
  return root
}

export function disposeModel(root) {
  const materials = new Set(root.userData.materials || [])
  root.traverse(object => {
    object.geometry?.dispose()
    if (object.material) for (const material of Array.isArray(object.material) ? object.material : [object.material]) materials.add(material)
  })
  materials.forEach(material => material.dispose())
}
