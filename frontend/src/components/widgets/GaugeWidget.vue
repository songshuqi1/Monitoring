<template>
  <div class="gauge-w" ref="containerRef">
    <canvas ref="canvasRef"></canvas>
    <div class="g-value">{{ displayValue }}</div>
    <div class="g-unit">{{ widget.config.unit || '' }}</div>
  </div>
</template>

<script setup>
import { ref, computed, onMounted, onUnmounted, watch, nextTick, inject } from 'vue'
import { useMonitorStore } from '../../store/index.js'

const props = defineProps({ widget: { type: Object, required: true } })
const store = useMonitorStore()
const containerRef = ref(null)
const canvasRef = ref(null)
const canvasZoom = inject('canvasZoom', ref(1))
const isCanvasZooming = inject('isCanvasZooming', ref(false))
const zoomRefreshTick = inject('zoomRefreshTick', ref(0))
const varId = computed(() => props.widget.config.varId)
const minVal = computed(() => props.widget.config.min ?? 0)
const maxVal = computed(() => props.widget.config.max ?? 100)
const currentValue = computed(() => {
  if (varId.value == null) return 0
  const num = Number(store.realtimeData[varId.value]?.value)
  return Number.isFinite(num) ? num : 0
})
const displayValue = computed(() => currentValue.value.toFixed(1))

let resizeObserver = null
let redrawFrame = 0
let zoomRedrawPending = false

function renderScale() {
  const dpr = window.devicePixelRatio || 1
  const zoom = Number(canvasZoom.value) || 1
  return Math.max(1, Math.min(6, dpr * zoom))
}

function draw() {
  const canvas = canvasRef.value
  if (!canvas) return
  const rect = containerRef.value.getBoundingClientRect()
  const zoom = Number(canvasZoom.value) || 1
  const cssWidth = containerRef.value.clientWidth || Math.round(rect.width / zoom)
  const cssHeight = containerRef.value.clientHeight || Math.round(rect.height / zoom)
  const dpr = renderScale()
  canvas.width = Math.max(1, Math.round(cssWidth * dpr))
  canvas.height = Math.max(1, Math.round(cssHeight * dpr))
  canvas.style.width = cssWidth + 'px'
  canvas.style.height = cssHeight + 'px'
  const ctx = canvas.getContext('2d')
  const w = canvas.width, h = canvas.height

  ctx.clearRect(0, 0, w, h)
  const cx = w / 2
  const cy = h * 0.58
  const r = Math.min(cx * 0.7, cy * 0.75)
  const startA = Math.PI * 0.75
  const endA = Math.PI * 2.25
  const range = endA - startA

  // Background arc
  ctx.beginPath(); ctx.arc(cx, cy, r, startA, endA)
  ctx.strokeStyle = '#e7ebf1'
  ctx.lineWidth = 10 * dpr; ctx.lineCap = 'round'; ctx.stroke()

  // Value arc
  const ratio = Math.max(0, Math.min(1, (currentValue.value - minVal.value) / (maxVal.value - minVal.value)))
  const valAngle = startA + range * ratio
  ctx.beginPath(); ctx.arc(cx, cy, r, startA, valAngle)
  let color = '#4f6fb8'
  if (ratio > 0.85) color = '#c2414b'
  else if (ratio > 0.7) color = '#a66a1f'
  ctx.strokeStyle = color
  ctx.lineWidth = 10 * dpr; ctx.lineCap = 'round'; ctx.stroke()

  // Tick marks
  for (let i = 0; i <= 10; i++) {
    const a = startA + (range / 10) * i
    const inner = r - 6 * dpr, outer = r + (i % 5 === 0 ? 8 : 5) * dpr
    ctx.beginPath()
    ctx.moveTo(cx + Math.cos(a) * inner, cy + Math.sin(a) * inner)
    ctx.lineTo(cx + Math.cos(a) * outer, cy + Math.sin(a) * outer)
    ctx.strokeStyle = i % 5 === 0 ? '#98a2b3' : '#d8dee8'
    ctx.lineWidth = i % 5 === 0 ? 1.5 * dpr : 1 * dpr
    ctx.stroke()
  }

  // Needle
  const needleLen = r * 0.7
  ctx.beginPath()
  ctx.moveTo(cx, cy)
  ctx.lineTo(cx + Math.cos(valAngle) * needleLen, cy + Math.sin(valAngle) * needleLen)
  ctx.strokeStyle = '#344054'
  ctx.lineWidth = 2 * dpr; ctx.stroke()
  ctx.beginPath(); ctx.arc(cx, cy, 4 * dpr, 0, Math.PI * 2)
  ctx.fillStyle = '#344054'; ctx.fill()
}

function requestRedraw() {
  if (isCanvasZooming.value) {
    zoomRedrawPending = true
    return
  }
  if (redrawFrame) return
  redrawFrame = requestAnimationFrame(() => {
    redrawFrame = 0
    draw()
  })
}

onMounted(() => {
  nextTick(draw)
  resizeObserver = new ResizeObserver(requestRedraw)
  if (containerRef.value) resizeObserver.observe(containerRef.value)
})
onUnmounted(() => {
  if (resizeObserver) resizeObserver.disconnect()
  if (redrawFrame) cancelAnimationFrame(redrawFrame)
})
watch(currentValue, requestRedraw)
watch(canvasZoom, requestRedraw)
watch(isCanvasZooming, (zooming) => {
  if (!zooming && zoomRedrawPending) {
    zoomRedrawPending = false
    requestRedraw()
  }
})
watch(zoomRefreshTick, requestRedraw)
</script>

<style scoped>
.gauge-w {
  width: 100%; height: 100%;
  position: relative;
  display: flex; flex-direction: column; align-items: center;
}
canvas { position: absolute; inset: 0; width: 100%; height: 100%; }
.g-value {
  position: relative; z-index: 1;
  font-family: var(--font-mono); font-size: var(--fs-xl);
  font-weight: var(--fw-bold); color: var(--accent);
  margin-top: auto; margin-bottom: 2px;
}
.g-unit {
  position: relative; z-index: 1;
  font-size: var(--fs-xs); color: var(--text-placeholder);
  margin-bottom: 6px;
}
</style>
