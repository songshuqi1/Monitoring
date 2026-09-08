<template>
  <div ref="host" class="model-preview-card" role="tooltip">
    <div class="model-preview-copy">
      <strong>{{ model.name }}</strong>
      <span>拖入画布或点击添加</span>
    </div>
  </div>
</template>

<script setup>
import { onBeforeUnmount, onMounted, ref, watch } from 'vue'
import * as THREE from 'three'
import { buildIndustrialModel, disposeModel } from '../../services/industrialModels.js'

const props = defineProps({ model: { type: Object, required: true } })
const host = ref(null)
let renderer, scene, camera, object, frame, observer

function replaceModel() {
  if (!scene) return
  if (object) { scene.remove(object); disposeModel(object) }
  object = buildIndustrialModel({ id: 'library-preview', type: props.model.type, color: props.model.color, pipeLength: 4 })
  scene.add(object)
  const bounds = new THREE.Box3().setFromObject(object)
  const center = bounds.getCenter(new THREE.Vector3())
  const size = bounds.getSize(new THREE.Vector3())
  object.position.sub(center)
  object.position.y += size.y / 2
  const distance = Math.max(size.x, size.y, size.z) * 1.65 + 1.5
  camera.position.set(distance, distance * 0.72, distance)
  camera.lookAt(0, Math.max(0.4, size.y * 0.42), 0)
}

function resize() {
  if (!renderer || !host.value?.clientWidth || !host.value?.clientHeight) return
  const width = host.value.clientWidth, height = host.value.clientHeight
  renderer.setSize(width, height, false)
  camera.aspect = width / height
  camera.updateProjectionMatrix()
}

function animate() {
  frame = requestAnimationFrame(animate)
  if (object) object.rotation.y += 0.004
  renderer?.render(scene, camera)
}

watch(() => props.model.type, replaceModel)
onMounted(() => {
  renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true, powerPreference: 'low-power' })
  renderer.setPixelRatio(Math.min(devicePixelRatio, 1.5))
  renderer.shadowMap.enabled = true
  renderer.outputColorSpace = THREE.SRGBColorSpace
  renderer.domElement.setAttribute('aria-hidden', 'true')
  host.value.prepend(renderer.domElement)
  scene = new THREE.Scene()
  camera = new THREE.PerspectiveCamera(34, 1, 0.1, 500)
  scene.add(new THREE.HemisphereLight('#ffffff', '#7690a1', 2.8))
  const light = new THREE.DirectionalLight('#fff5e8', 3.5)
  light.position.set(7, 12, 9); light.castShadow = true; scene.add(light)
  const ground = new THREE.Mesh(new THREE.CircleGeometry(7, 48), new THREE.MeshStandardMaterial({ color: '#dce6ec', roughness: 0.92 }))
  ground.rotation.x = -Math.PI / 2; ground.position.y = -0.04; ground.receiveShadow = true; scene.add(ground)
  replaceModel(); resize(); animate()
  observer = new ResizeObserver(resize); observer.observe(host.value)
})
onBeforeUnmount(() => {
  cancelAnimationFrame(frame); observer?.disconnect()
  if (object) disposeModel(object)
  if (object) scene?.remove(object)
  scene?.traverse(item => { if (item !== object) { item.geometry?.dispose(); item.material?.dispose?.() } })
  renderer?.dispose(); renderer?.forceContextLoss(); renderer?.domElement.remove()
})
</script>

<style scoped>
.model-preview-card { position: absolute; z-index: 30; width: 250px; height: 210px; overflow: hidden; border: 1px solid #b8cbd8; border-radius: 12px; background: linear-gradient(145deg, #f9fcfe, #e8f0f5); box-shadow: 0 16px 38px rgba(38, 61, 78, .2); pointer-events: none; }
.model-preview-card::after { content: ''; position: absolute; inset: 0; border: 1px solid rgba(255,255,255,.85); border-radius: inherit; }
.model-preview-card canvas { display: block; width: 100%; height: 100%; }
.model-preview-copy { position: absolute; z-index: 2; left: 12px; right: 12px; bottom: 10px; display: flex; align-items: baseline; justify-content: space-between; gap: 12px; padding: 8px 10px; border: 1px solid rgba(255,255,255,.8); border-radius: 7px; background: rgba(255,255,255,.88); color: #36566d; backdrop-filter: blur(8px); }
.model-preview-copy strong { font-size: 15px; }
.model-preview-copy span { color: #7890a1; font-size: 12px; white-space: nowrap; }
</style>
