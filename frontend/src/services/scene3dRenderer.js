import * as THREE from 'three'
import { OrbitControls } from 'three/addons/controls/OrbitControls.js'
import { TransformControls } from 'three/addons/controls/TransformControls.js'
import { CSS2DRenderer, CSS2DObject } from 'three/addons/renderers/CSS2DRenderer.js'
import { RoomEnvironment } from 'three/addons/environments/RoomEnvironment.js'
import { buildIndustrialModel, disposeModel } from './industrialModels.js'

const statusColors = { unbound: '#8797a7', missing: '#c48135', offline: '#8292a3', stale: '#bf862e', fault: '#dd5260', running: '#2aaf83', stopped: '#708398', good: '#2aaf83' }

export function createSceneRenderer(host, events) {
  const renderer = new THREE.WebGLRenderer({ antialias: true, alpha: false, powerPreference: 'high-performance' })
  renderer.setPixelRatio(Math.min(devicePixelRatio, 1.75))
  renderer.shadowMap.enabled = true
  renderer.shadowMap.type = THREE.PCFSoftShadowMap
  renderer.toneMapping = THREE.ACESFilmicToneMapping
  renderer.toneMappingExposure = 1.3
  renderer.domElement.className = 'industrial-webgl'
  renderer.domElement.setAttribute('aria-label', '3D 工业监控场景')
  host.appendChild(renderer.domElement)
  const labels = new CSS2DRenderer()
  labels.domElement.className = 'industrial-label-layer'
  host.appendChild(labels.domElement)
  const scene = new THREE.Scene()
  const camera = new THREE.PerspectiveCamera(40, 1, 0.1, 2000)
  camera.position.set(18, 15, 20)
  const controls = new OrbitControls(camera, host)
  controls.mouseButtons.MIDDLE = THREE.MOUSE.PAN
  controls.enableDamping = true
  controls.dampingFactor = 0.09
  controls.maxPolarAngle = Math.PI * 0.495
  controls.minDistance = 2
  controls.maxDistance = 500
  controls.target.set(0, 1, 0)
  const transform = new TransformControls(camera, host)
  transform.setSize(0.85)
  scene.add(transform.getHelper())
  const content = new THREE.Group()
  scene.add(content)
  const ambient = new THREE.HemisphereLight('#eef8ff', '#89929a', 2.3)
  scene.add(ambient)
  const sun = new THREE.DirectionalLight('#fff4e8', 3.3)
  sun.position.set(12, 22, 10)
  sun.castShadow = true
  sun.shadow.mapSize.set(2048, 2048)
  Object.assign(sun.shadow.camera, { left: -25, right: 25, top: 25, bottom: -25, far: 90 })
  sun.shadow.normalBias = 0.04
  scene.add(sun)
  const room = new RoomEnvironment()
  const pmrem = new THREE.PMREMGenerator(renderer)
  const environment = pmrem.fromScene(room, 0.04)
  scene.environment = environment.texture
  room.dispose(); pmrem.dispose()
  const ground = new THREE.Mesh(new THREE.PlaneGeometry(2000, 2000), new THREE.MeshStandardMaterial({ color: '#dde5e9', roughness: 0.92, metalness: 0.05 }))
  ground.rotation.x = -Math.PI / 2
  ground.position.y = -0.16
  ground.receiveShadow = true
  scene.add(ground)
  const grid = new THREE.GridHelper(80, 80, '#b5c4cd', '#cbd6de')
  grid.position.y = -0.145
  grid.material.transparent = true
  grid.material.opacity = 0.5
  scene.add(grid)
  const selection = new THREE.BoxHelper(new THREE.Object3D(), '#458cb6')
  selection.visible = false
  scene.add(selection)
  const objects = new Map()
  let nodes = [], selectedId = null, editable = true, dragging = false, panelResizing = false, destroyed = false, raf = 0, resizeRaf = 0, lastFrame = 0, lastRuntime = 0, renderWidth = 0, renderHeight = 0
  let cameraTransition = null
  let config = { labels: true, quality: 'standard' }
  let runtimeCache = new Map()
  let pointerStart = null, lastClick = null
  const raycaster = new THREE.Raycaster()
  const pointer = new THREE.Vector2()

  function cameraSnapshot() { return { position: camera.position.toArray(), target: controls.target.toArray() } }
  function persistCamera() { if (!destroyed) events.onCamera(cameraSnapshot()) }
  function cancelCameraTransition() { cameraTransition = null }
  controls.addEventListener('end', persistCamera)
  controls.addEventListener('start', cancelCameraTransition)
  transform.addEventListener('dragging-changed', event => {
    dragging = event.value
    controls.enabled = !dragging
    if (!dragging && transform.object) {
      const o = transform.object
      events.onTransform(o.userData.nodeId, {
        position: o.position.toArray(), rotation: [o.rotation.x, o.rotation.y, o.rotation.z].map(THREE.MathUtils.radToDeg), scale: o.scale.toArray()
      })
    }
  })
  transform.addEventListener('objectChange', () => { if (transform.object) selection.setFromObject(transform.object) })

  function setSelected(id) {
    selectedId = id
    const object = objects.get(id)?.object
    transform.detach()
    selection.visible = editable && !!object && object.visible
    if (selection.visible) {
      selection.setFromObject(object)
      if (editable && !nodes.find(node => node.id === id)?.locked) transform.attach(object)
    }
    objects.forEach((entry, key) => entry.label.element.classList.toggle('selected', key === id))
  }

  function setScene(next) {
    nodes = next.nodes
    config = next.environment
    const dark = config.theme === 'dark'
    scene.background = new THREE.Color(dark ? '#142333' : '#e7edf1')
    scene.fog = new THREE.Fog(dark ? '#142333' : '#e7edf1', 60, 160)
    ground.material.color.set(dark ? '#080f18' : '#dde5e9')
    grid.material.opacity = dark ? 0.16 : 0.36
    sun.intensity = dark ? 2.0 : 2.8
    renderer.toneMappingExposure = dark ? 1 : 1.15
    ambient.intensity = dark ? 1.65 : 2.3
    grid.visible = config.grid
    renderer.setPixelRatio(Math.min(devicePixelRatio, config.quality === 'smooth' ? 1 : config.quality === 'fine' ? 2 : 1.75))
    renderer.shadowMap.enabled = config.quality !== 'smooth'
    for (const [id, entry] of objects) {
      if (!nodes.some(node => node.id === id)) {
        if (transform.object === entry.object) transform.detach()
        entry.label.element.remove(); content.remove(entry.object); disposeModel(entry.object); objects.delete(id)
      }
    }
    nodes.forEach(node => {
      const signature = `${node.type}:${node.color}:${node.pipeLength}`
      let entry = objects.get(node.id)
      if (entry && entry.signature !== signature) {
        if (transform.object === entry.object) transform.detach()
        entry.label.element.remove(); content.remove(entry.object); disposeModel(entry.object); objects.delete(node.id); entry = null
      }
      if (!entry) {
        const object = buildIndustrialModel(node)
        const element = document.createElement('button')
        element.type = 'button'; element.className = 'industrial-device-label'
        const name = document.createElement('strong'), detail = document.createElement('span')
        element.append(name, detail)
        element.addEventListener('pointerdown', e => { if (e.button === 0) e.stopPropagation() })
        element.addEventListener('auxclick', e => e.preventDefault())
        element.addEventListener('click', () => events.onSelect(node.id))
        element.addEventListener('dblclick', e => { e.stopPropagation(); events.onSelect(node.id); focus(node.id) })
        const label = new CSS2DObject(element)
        label.position.set(0, object.userData.height + 0.2, 0)
        object.add(label)
        content.add(object)
        entry = { object, label, name, detail, signature }
        objects.set(node.id, entry)
      }
      entry.object.position.set(...node.position)
      entry.object.rotation.set(...node.rotation.map(THREE.MathUtils.degToRad))
      entry.object.scale.set(...node.scale)
      entry.object.visible = node.visible
      entry.node = node
      entry.name.textContent = node.name
      entry.name.style.fontSize = `${node.labelFontSize ?? config.labelFontSize ?? 14}px`
      entry.label.visible = config.labels
    })
    lastRuntime = 0
    setSelected(selectedId)
  }

  function restoreCamera(value) {
    cancelCameraTransition()
    camera.position.fromArray(value.position)
    controls.target.fromArray(value.target)
    if (camera.position.distanceTo(controls.target) < 0.5) camera.position.add(new THREE.Vector3(12, 10, 12))
    controls.update()
  }
  function focus(id = null, view = 'iso') {
    const targetObject = id ? objects.get(id)?.object : null
    const bounds = new THREE.Box3()
    if (targetObject) bounds.setFromObject(targetObject)
    else objects.forEach(entry => { if (entry.object.visible) bounds.expandByObject(entry.object) })
    if (bounds.isEmpty()) bounds.setFromCenterAndSize(new THREE.Vector3(0, 1, 0), new THREE.Vector3(12, 6, 12))
    const center = bounds.getCenter(new THREE.Vector3())
    const size = bounds.getSize(new THREE.Vector3())
    const distance = Math.max(5, size.length() * 1.35 / Math.min(camera.aspect, 1))
    const direction = view === 'top' ? new THREE.Vector3(0, 1, 0.001) : view === 'front' ? new THREE.Vector3(0, 0.25, 1) : view === 'side' ? new THREE.Vector3(1, 0.25, 0) : new THREE.Vector3(1, 0.85, 1)
    const destination = center.clone().add(direction.normalize().multiplyScalar(distance))
    cameraTransition = {
      startedAt: performance.now(), duration: 850,
      fromPosition: camera.position.clone(), fromTarget: controls.target.clone(),
      toPosition: destination, toTarget: center
    }
  }
  function pointerDown(event) {
    cancelCameraTransition()
    if (event.button === 1) event.preventDefault()
    pointerStart = { x: event.clientX, y: event.clientY, axis: !!transform.axis }
  }
  function pointerUp(event) {
    if (!pointerStart || event.button !== 0 || dragging || pointerStart.axis || transform.axis || Math.hypot(event.clientX - pointerStart.x, event.clientY - pointerStart.y) > 5) return
    const rect = renderer.domElement.getBoundingClientRect()
    pointer.set((event.clientX - rect.left) / rect.width * 2 - 1, -(event.clientY - rect.top) / rect.height * 2 + 1)
    raycaster.setFromCamera(pointer, camera)
    const hit = raycaster.intersectObjects([...objects.values()].filter(e => e.object.visible).map(e => e.object), true)[0]
    let object = hit?.object
    while (object && !object.userData.nodeId) object = object.parent
    const nodeId = object?.userData.nodeId || null
    events.onSelect(nodeId)
    const now = performance.now()
    if (nodeId && lastClick?.id === nodeId && now - lastClick.time < 360) focus(nodeId)
    lastClick = nodeId ? { id: nodeId, time: now } : null
    pointerStart = null
  }
  function resize(force = false) {
    if (destroyed || !host.clientWidth || !host.clientHeight) return
    const width = Math.round(host.clientWidth), height = Math.round(host.clientHeight)
    if (!force && width === renderWidth && height === renderHeight) return
    renderWidth = width; renderHeight = height
    renderer.setSize(width, height, false)
    labels.setSize(width, height)
    camera.aspect = width / height
    camera.updateProjectionMatrix()
    renderer.render(scene, camera)
    labels.render(scene, camera)
  }
  function scheduleResize() {
    if (resizeRaf) return
    resizeRaf = requestAnimationFrame(() => { resizeRaf = 0; resize() })
  }
  const observer = new ResizeObserver(scheduleResize)
  observer.observe(host)
  host.addEventListener('pointerdown', pointerDown)
  host.addEventListener('pointerup', pointerUp)
  function contextLost(event) { event.preventDefault(); cancelAnimationFrame(raf); events.onError('3D 图形上下文已丢失，请切回 2D 后重新进入 3D。') }
  renderer.domElement.addEventListener('webglcontextlost', contextLost)

  function frame(time) {
    if (destroyed) return
    raf = requestAnimationFrame(frame)
    if (document.hidden) { lastFrame = time; return }
    if (time - lastFrame < (config.quality === 'smooth' ? 32 : 15)) return
    const delta = Math.min((time - lastFrame) / 1000, 0.05)
    lastFrame = time
    if (cameraTransition) {
      const elapsed = Math.max(0, time - cameraTransition.startedAt)
      const progress = Math.min(1, elapsed / cameraTransition.duration)
      const eased = progress < 0.5 ? 4 * progress ** 3 : 1 - (-2 * progress + 2) ** 3 / 2
      camera.position.lerpVectors(cameraTransition.fromPosition, cameraTransition.toPosition, eased)
      controls.target.lerpVectors(cameraTransition.fromTarget, cameraTransition.toTarget, eased)
      if (progress >= 1) { cameraTransition = null; persistCamera() }
    }
    controls.update()
    if (!lastRuntime || time - lastRuntime > 200) {
      runtimeCache = new Map(nodes.map(node => [node.id, events.getRuntime(node)]))
      objects.forEach((entry, id) => {
        const runtime = runtimeCache.get(id)
        if (!runtime) return
        entry.detail.textContent = ['platform', 'building'].includes(entry.node.type) ? '场景设施' : `${runtime.label}  ·  ${runtime.text}`
        entry.label.element.dataset.state = runtime.state
        const lamp = entry.object.userData.indicator
        if (lamp) { lamp.material.color.set(statusColors[runtime.state]); lamp.material.emissive.set(statusColors[runtime.state]) }
        const liquid = entry.object.userData.liquid
        if (liquid) { liquid.visible = runtime.level > 0; liquid.scale.y = Math.max(0.005, runtime.level * 3.4); liquid.position.y = 0.78 + liquid.scale.y / 2 }
      })
      lastRuntime = time
    }
    const occupied = []
    const sorted = [...objects.entries()].sort(([a], [b]) => Number(b === selectedId) - Number(a === selectedId))
    sorted.forEach(([id, entry]) => {
      const node = entry.node, runtime = runtimeCache.get(id)
      const rotor = entry.object.userData.rotor
      if (rotor && runtime?.running && node.type !== 'valve') {
        if (node.type === 'pipe') {
          rotor.children.forEach(arrow => { arrow.position.x += delta * 1.2; if (arrow.position.x > node.pipeLength / 2) arrow.position.x = -node.pipeLength / 2 })
        } else rotor.rotation[rotor.userData.axis || 'z'] += delta * 3
      }
      if (rotor && node.type === 'valve') rotor.rotation.y = runtime?.running ? Math.PI / 2 : 0
      const p = entry.label.getWorldPosition(new THREE.Vector3()).project(camera)
      const x = (p.x + 1) * host.clientWidth / 2, y = (1 - p.y) * host.clientHeight / 2
      const element = entry.label.element
      if (element.offsetWidth && element.offsetHeight) entry.labelSize = { width: element.offsetWidth, height: element.offsetHeight }
      const { width, height } = entry.labelSize || { width: 160, height: 56 }
      const visible = config.labels && entry.object.visible && (!['platform', 'building', 'pipe'].includes(node.type) || id === selectedId) && p.z > -1 && p.z < 1 && x > 0 && x < host.clientWidth && y > 0 && y < host.clientHeight && !occupied.some(rect => Math.abs(rect.x - x) < (rect.width + width) / 2 + 8 && Math.abs(rect.y - y) < (rect.height + height) / 2 + 8)
      entry.label.visible = visible
      if (visible) occupied.push({ x, y, width, height })
    })
    if (selection.visible && transform.object) selection.setFromObject(transform.object)
    renderer.render(scene, camera)
    labels.render(scene, camera)
  }
  resize()
  raf = requestAnimationFrame(frame)
  return {
    setScene, setSelected, restoreCamera, focus,
    groundPosition(clientX, clientY) {
      const rect = renderer.domElement.getBoundingClientRect()
      pointer.set((clientX - rect.left) / rect.width * 2 - 1, -(clientY - rect.top) / rect.height * 2 + 1)
      raycaster.setFromCamera(pointer, camera)
      const point = raycaster.ray.intersectPlane(new THREE.Plane(new THREE.Vector3(0, 1, 0), 0), new THREE.Vector3())
      return point ? [Math.round(point.x * 2) / 2, 0, Math.round(point.z * 2) / 2] : undefined
    },
    setMode(mode) { transform.setMode(mode) },
    setSnap(enabled) { transform.setTranslationSnap(enabled ? 0.5 : null); transform.setRotationSnap(enabled ? Math.PI / 12 : null); transform.setScaleSnap(enabled ? 0.1 : null) },
    setEditable(value) { editable = value; setSelected(selectedId) },
    setResizing(value) {
      panelResizing = Boolean(value)
      host.classList.toggle('is-panel-resizing', panelResizing)
      if (!panelResizing) {
        if (resizeRaf) cancelAnimationFrame(resizeRaf)
        resizeRaf = 0
        scheduleResize()
      }
    },
    dispose() {
      persistCamera(); destroyed = true; cancelAnimationFrame(raf); cancelAnimationFrame(resizeRaf); observer.disconnect()
      controls.removeEventListener('end', persistCamera)
      controls.removeEventListener('start', cancelCameraTransition)
      controls.dispose(); transform.dispose()
      objects.forEach(entry => { entry.label.element.remove(); disposeModel(entry.object) })
      objects.clear()
      ground.geometry.dispose(); ground.material.dispose(); grid.geometry.dispose(); grid.material.dispose()
      selection.geometry.dispose(); selection.material.dispose(); environment.dispose(); sun.shadow.dispose()
      host.removeEventListener('pointerdown', pointerDown)
      host.removeEventListener('pointerup', pointerUp)
      renderer.domElement.removeEventListener('webglcontextlost', contextLost)
      renderer.dispose(); renderer.forceContextLoss(); renderer.domElement.remove(); labels.domElement.remove()
    }
  }
}
