<template>
  <div class="trend-chart" ref="containerRef">
    <div class="trend-main">
      <canvas ref="canvasRef"></canvas>
        <div v-if="!hasData" class="no-data">等待数据...</div>
    </div>
    <div class="trend-legend" v-if="showLegend && visibleVars.length > 0">
      <div v-for="(item, idx) in visibleVars" :key="item.id" class="trend-legend-item">
        <span class="tl-dot" :style="{ background: colors[idx % colors.length] }"></span>
        <span class="tl-name">{{ item.name }}</span>
        <span class="tl-value" :style="{ color: colors[idx % colors.length] }">{{ item.value }}</span>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, computed, onMounted, onUnmounted, watch, nextTick, inject } from 'vue'
import { useMonitorStore } from '../../store/index.js'

const props = defineProps({ widget: { type: Object, required: true } })
const store = useMonitorStore()
const containerRef = ref(null)
const canvasRef = ref(null)
const hasData = ref(false)
const canvasZoom = inject('canvasZoom', ref(1))
const isCanvasZooming = inject('isCanvasZooming', ref(false))
const zoomRefreshTick = inject('zoomRefreshTick', ref(0))

// 从 store 读取已有缓冲 (跨页面切换后保留)
const dataBuffers = store.trendBuffers
const maxPoints = computed(() => store.TREND_MAX_POINTS || 200)
const timeWindow = computed(() => normalizeTimeWindow(props.widget.config.timeWindow))
let timer = null
let redrawFrame = 0
let zoomRedrawPending = false

const colors = ['#4f6fb8', '#a66a1f', '#2f8f63', '#c2414b', '#6f5fb8', '#9b4d7d']
const showLegend = computed(() => props.widget?.type !== 'trendChartLite')

function renderScale() {
  const dpr = window.devicePixelRatio || 1
  const zoom = Number(canvasZoom.value) || 1
  return Math.max(1, Math.min(6, dpr * zoom))
}

function normalizeTimeWindow(value) {
  const n = Number(value)
  if (!Number.isFinite(n) || n <= 0) return 60000
  return n <= 1000 ? n * 1000 : n
}

function selectedVarIds() {
  const configVarIds = props.widget.config.varIds
  return (Array.isArray(configVarIds) && configVarIds.length > 0) ? configVarIds : []
}

const visibleVars = computed(() => {
  const varIds = selectedVarIds()
  return varIds.map((vid, idx) => {
    const v = store.variables.find(x => x.id === vid)
    if (!v) return null
    const dp = store.realtimeData[vid]
    const value = dp && typeof dp.value === 'number' ? dp.value.toFixed(1) : '-'
    return { id: vid, name: v.name, value }
  }).filter(Boolean)
})

function initCanvas() {
  const canvas = canvasRef.value
  if (!canvas) return
  const parent = canvas.parentElement
  if (!parent) return
  const zoom = Number(canvasZoom.value) || 1
  const rect = parent.getBoundingClientRect()
  const cssWidth = parent.clientWidth || Math.round(rect.width / zoom)
  const cssHeight = parent.clientHeight || Math.round(rect.height / zoom)
  const scale = renderScale()
  canvas.width = Math.max(1, Math.round(cssWidth * scale))
  canvas.height = Math.max(1, Math.round(cssHeight * scale))
  canvas.style.width = cssWidth + 'px'
  canvas.style.height = cssHeight + 'px'
}

function requestRedraw() {
  if (isCanvasZooming.value) {
    zoomRedrawPending = true
    return
  }
  if (redrawFrame) return
  redrawFrame = requestAnimationFrame(() => {
    redrawFrame = 0
    initCanvas()
    draw()
  })
}

function draw() {
  const canvas = canvasRef.value
  if (!canvas) return
  const ctx = canvas.getContext('2d')
  const dpr = renderScale()
  const w = canvas.width, h = canvas.height

  ctx.clearRect(0, 0, w, h)

  // chart background
  ctx.fillStyle = '#ffffff'
  ctx.fillRect(0, 0, w, h)

  const pad = { t: 20*dpr, r: 12*dpr, b: 24*dpr, l: 42*dpr }
  const pw = w - pad.l - pad.r
  const ph = h - pad.t - pad.b
  if (pw <= 0 || ph <= 0) return

  const now = Date.now()
  const tStart = now - timeWindow.value
  const varIds = selectedVarIds()

  if (varIds.length === 0) { hasData.value = false; return }

    let yMin, yMax, allEmpty = true
    const cfg = props.widget.config
    const chartFontSize = Math.max(9, Math.min(18, Number(cfg.chartFontSize) || 10))
    const hasManualY = typeof cfg.yMin === 'number' && typeof cfg.yMax === 'number'
    if (hasManualY) {
      yMin = cfg.yMin
      yMax = cfg.yMax
    } else {
      yMin = Infinity
      yMax = -Infinity
    }
    varIds.forEach(vid => {
    let buf = dataBuffers[vid]
    // 如果缓冲为空但有实时数据，即时补充
    if (!buf || buf.length === 0) {
      const dp = store.realtimeData[vid]
      if (dp && typeof dp.value === 'number') {
        if (!buf) { buf = []; dataBuffers[vid] = buf }
        const t0 = now - Math.min(10000, timeWindow.value)
        buf.push({ time: t0, value: dp.value })
        buf.push({ time: now, value: dp.value })
      }
    }
    // 这里不 return，继续执行下面的 Y 轴计算
    const b = dataBuffers[vid]
    if (!b || b.length === 0) return
    const windowed = b.filter(p => p.time >= tStart && p.time <= now)
    if (windowed.length === 0) return
    allEmpty = false
      if (!hasManualY) {
        windowed.forEach(p => { if (p.value < yMin) yMin = p.value; if (p.value > yMax) yMax = p.value })
      }
  })
  if (allEmpty || yMin === Infinity) { hasData.value = false; return }
    if (!hasManualY && yMin === yMax) { yMin -= 1; yMax += 1 }
  hasData.value = true

  // Grid lines
  ctx.strokeStyle = '#e7ebf1'
  ctx.lineWidth = 1
  for (let i = 0; i <= 4; i++) {
    const y = pad.t + (ph / 4) * i
    ctx.beginPath(); ctx.moveTo(pad.l, y); ctx.lineTo(w - pad.r, y); ctx.stroke()
  }

  // Y axis
  ctx.fillStyle = '#98a2b3'
  ctx.font = `${chartFontSize*dpr}px "Times New Roman", "SimSun", "宋体", serif`
  ctx.textAlign = 'right'
  ctx.textBaseline = 'middle'
  for (let i = 0; i <= 4; i++) {
    const val = yMax - (yMax - yMin) * (i / 4)
    ctx.fillText(val.toFixed(1), pad.l - 6*dpr, pad.t + (ph / 4) * i)
  }

  const timeRange = now - tStart

  varIds.forEach((vid, idx) => {
    const buf = dataBuffers[vid]
    if (!buf || buf.length < 2) return
    // 只绘制时间窗口内的点
    const windowed = buf.filter(p => p.time >= tStart && p.time <= now)
    if (windowed.length < 2) return
    const color = colors[idx % colors.length]
    ctx.strokeStyle = color
    ctx.lineWidth = 2 * dpr
    ctx.beginPath()
    let first = true
    windowed.forEach(p => {
      const x = pad.l + ((p.time - tStart) / timeRange) * pw
      const y = pad.t + (1 - (p.value - yMin) / (yMax - yMin)) * ph
      first ? ctx.moveTo(x, y) : ctx.lineTo(x, y)
      first = false
    })
    ctx.stroke()
  })

  // X axis time
  ctx.fillStyle = '#98a2b3'
  ctx.font = `${Math.max(9, chartFontSize - 1)*dpr}px "Times New Roman", "SimSun", "宋体", serif`
  ctx.textAlign = 'center'
  ctx.textBaseline = 'top'
  for (let i = 0; i <= 4; i++) {
    const t = tStart + (timeRange / 4) * i
    const x = pad.l + (pw / 4) * i
    const d = new Date(t)
    ctx.fillText(`${String(d.getHours()).padStart(2,'0')}:${String(d.getMinutes()).padStart(2,'0')}:${String(d.getSeconds()).padStart(2,'0')}`, x, h - 22*dpr)
  }
}

let resizeObserver = null

onMounted(() => {
  nextTick(() => { initCanvas(); draw() })
  resizeObserver = new ResizeObserver(() => { requestRedraw() })
  if (containerRef.value) resizeObserver.observe(containerRef.value)
  timer = setInterval(() => {
    if (!isCanvasZooming.value) draw()
  }, 200)
})

onUnmounted(() => {
  if (timer) clearInterval(timer)
  if (resizeObserver) resizeObserver.disconnect()
  if (redrawFrame) cancelAnimationFrame(redrawFrame)
})

watch(canvasZoom, () => { requestRedraw() })
watch(isCanvasZooming, (zooming) => {
  if (!zooming && zoomRedrawPending) {
    zoomRedrawPending = false
    requestRedraw()
  }
})
watch(zoomRefreshTick, () => { requestRedraw() })
</script>

<style scoped>
.trend-chart {
  width: 100%; height: 100%;
  position: relative;
  overflow: hidden;
  display: flex;
  flex-direction: row;
}
.trend-main {
  flex: 1;
  min-width: 0;
  position: relative;
  overflow: hidden;
}
canvas { display: block; width: 100%; height: 100%; }
.no-data {
  position: absolute; inset: 0;
  display: flex; align-items: center; justify-content: center;
  color: var(--text-placeholder); font-size: var(--fs-sm);
  background: rgba(255,255,255,0.86);
  backdrop-filter: blur(2px);
}
.trend-legend {
  width: 132px;
  flex-shrink: 0;
  border-left: 1px solid var(--border-light);
  padding: 10px;
  overflow-y: auto;
  display: flex;
  flex-direction: column;
  gap: 6px;
  background: var(--surface-muted);
}
.trend-legend-item {
  display: flex;
  align-items: center;
  gap: 6px;
  padding: 4px 0;
  font-size: 11px;
  line-height: 1.3;
}
.tl-dot {
  width: 8px;
  height: 8px;
  border-radius: 50%;
  flex-shrink: 0;
}
.tl-name {
  flex: 1;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
  color: var(--text-tertiary);
}
.tl-value {
  font-family: var(--font-mono);
  font-size: 11px;
  font-weight: var(--fw-semibold, 600);
  text-align: right;
  min-width: 40px;
}
.tl-empty {
  color: var(--text-placeholder);
  font-size: 11px;
  padding: 4px 0;
  text-align: center;
}
</style>
