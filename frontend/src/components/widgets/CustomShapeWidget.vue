<template>
  <div class="custom-shape-widget">
    <svg
      class="custom-shape-svg"
      :viewBox="`0 0 ${definition.width} ${definition.height}`"
      preserveAspectRatio="none"
      aria-hidden="true"
    >
      <template v-for="shape in definition.shapes" :key="shape.id">
        <g :transform="shapeTransform(shape)">
          <rect
            v-if="['rect', 'square', 'round-rect', 'capsule'].includes(shape.kind)"
            :x="shape.x"
            :y="shape.y"
            :width="Math.max(0, Number(shape.w) || 0)"
            :height="Math.max(0, Number(shape.h) || 0)"
            :rx="shape.kind === 'capsule' ? Math.max(0, shape.h / 2) : (shape.radius || 0)"
            :fill="shapeFill(shape)"
            :stroke="shape.stroke"
            :stroke-width="shape.strokeWidth"
          />
          <ellipse
            v-else-if="shape.kind === 'circle' || shape.kind === 'ellipse'"
            :cx="shape.x + shape.w / 2"
            :cy="shape.y + shape.h / 2"
            :rx="Math.max(0, shape.w / 2)"
            :ry="Math.max(0, shape.h / 2)"
            :fill="shapeFill(shape)"
            :stroke="shape.stroke"
            :stroke-width="shape.strokeWidth"
          />
          <polygon
            v-else-if="polygonShapeKinds.includes(shape.kind)"
            :points="polygonPoints(shape)"
            :fill="shapeFill(shape)"
            :stroke="shape.stroke"
            :stroke-width="shape.strokeWidth"
          />
          <path
            v-else-if="pathShapeKinds.includes(shape.kind)"
            :d="shapePath(shape)"
            :fill="pathShapeFill(shape)"
            :stroke="shape.stroke"
            :stroke-width="Math.max(1, shape.strokeWidth)"
            stroke-linecap="round"
            stroke-linejoin="round"
          />
          <g v-else-if="threeDShapeKinds.includes(shape.kind)">
            <path
              v-for="part in threeDShapeParts(shape, shapeFill(shape))"
              :key="part.key"
              :d="part.d"
              :fill="part.fill"
              :fill-opacity="part.fillOpacity"
              :stroke="part.stroke || shape.stroke"
              :stroke-opacity="part.strokeOpacity"
              :stroke-width="Math.max(1, shape.strokeWidth)"
              stroke-linejoin="round"
              stroke-linecap="round"
            />
          </g>
          <line
            v-else-if="shape.kind === 'line'"
            :x1="shape.x"
            :y1="shape.y + shape.h / 2"
            :x2="shape.x + shape.w"
            :y2="shape.y + shape.h / 2"
            :stroke="shape.stroke"
            :stroke-width="Math.max(1, shape.strokeWidth)"
            stroke-linecap="round"
          />
          <line
            v-else-if="shape.kind === 'line-v'"
            :x1="shape.x + shape.w / 2"
            :y1="shape.y"
            :x2="shape.x + shape.w / 2"
            :y2="shape.y + shape.h"
            :stroke="shape.stroke"
            :stroke-width="Math.max(1, shape.strokeWidth)"
            stroke-linecap="round"
          />
          <line
            v-else-if="shape.kind === 'slash'"
            :x1="shape.x"
            :y1="shape.y + shape.h"
            :x2="shape.x + shape.w"
            :y2="shape.y"
            :stroke="shape.stroke"
            :stroke-width="Math.max(1, shape.strokeWidth)"
            stroke-linecap="round"
          />
          <line
            v-else-if="shape.kind === 'backslash'"
            :x1="shape.x"
            :y1="shape.y"
            :x2="shape.x + shape.w"
            :y2="shape.y + shape.h"
            :stroke="shape.stroke"
            :stroke-width="Math.max(1, shape.strokeWidth)"
            stroke-linecap="round"
          />
          <g v-else-if="shape.kind === 'cross'">
            <line
              :x1="shape.x"
              :y1="shape.y + shape.h / 2"
              :x2="shape.x + shape.w"
              :y2="shape.y + shape.h / 2"
              :stroke="shape.stroke"
              :stroke-width="Math.max(1, shape.strokeWidth)"
              stroke-linecap="round"
            />
            <line
              :x1="shape.x + shape.w / 2"
              :y1="shape.y"
              :x2="shape.x + shape.w / 2"
              :y2="shape.y + shape.h"
              :stroke="shape.stroke"
              :stroke-width="Math.max(1, shape.strokeWidth)"
              stroke-linecap="round"
            />
          </g>
        </g>
      </template>
    </svg>
    <div v-if="label && !hideName" class="custom-shape-label" :style="labelStyle">{{ label }}</div>
  </div>
</template>

<script setup>
import { computed } from 'vue'
import { useMonitorStore } from '../../store/index.js'

const props = defineProps({
  widget: { type: Object, required: true }
})

const store = useMonitorStore()

const definition = computed(() => {
  const source = props.widget.config?.definition || {}
  return {
    width: Math.max(1, Number(source.width) || Number(props.widget.w) || 180),
    height: Math.max(1, Number(source.height) || Number(props.widget.h) || 120),
    shapes: Array.isArray(source.shapes) ? source.shapes : [],
    bindVariable: Boolean(source.bindVariable || props.widget.config?.bindVariable),
    statusOnColor: source.statusOnColor || props.widget.config?.statusOnColor || '#22c55e',
    statusOffColor: source.statusOffColor || props.widget.config?.statusOffColor || '#ef4444'
  }
})

const dataPoint = computed(() => {
  const id = props.widget.config?.varId
  return id == null ? null : store.realtimeData[id]
})

const lineShapeKinds = ['line', 'line-v', 'slash', 'backslash', 'cross', 'arc', 'pipe-elbow', 'pipe-tee']
const pathShapeKinds = ['arrow-right', 'arrow-left', 'arc', 'pipe-elbow', 'pipe-tee']
const filledPathShapeKinds = ['arrow-right', 'arrow-left']
const threeDShapeKinds = ['cube', 'cylinder', 'cylinder-h', 'cone', 'frustum', 'tri-prism']
const polygonShapeKinds = ['triangle', 'right-triangle', 'trapezoid', 'parallelogram', 'diamond', 'pentagon', 'hexagon', 'octagon', 'star', 'chevron']

function readBinaryState(value) {
  if (typeof value === 'boolean') return value
  if (typeof value === 'number') return Number.isFinite(value) && value > 0
  const text = String(value ?? '').trim().toLowerCase()
  if (['1', 'true', 'on', 'open', 'run', 'running', 'start', 'yes', '开', '开启', '打开', '运行', '启动'].includes(text)) return true
  if (['0', 'false', 'off', 'close', 'closed', 'stop', 'stopped', 'no', '关', '关闭', '停止'].includes(text)) return false
  const numeric = Number(text)
  return Number.isFinite(numeric) && numeric > 0
}

const statusColor = computed(() => {
  if (!definition.value.bindVariable || !dataPoint.value || dataPoint.value.quality !== 'GOOD') return null
  return readBinaryState(dataPoint.value.value)
    ? definition.value.statusOnColor
    : definition.value.statusOffColor
})

function shapeFill(shape) {
  if (statusColor.value && shape.bindStatus !== false && !lineShapeKinds.includes(shape.kind)) {
    return statusColor.value
  }
  if (['arrow-right', 'arrow-left'].includes(shape.kind) && (!shape.fill || shape.fill === 'transparent')) {
    return '#263544'
  }
  return shape.fill || 'transparent'
}

function regularPolygonPoints(shape, sides, rotationDeg = -90) {
  const cx = shape.x + shape.w / 2
  const cy = shape.y + shape.h / 2
  const rx = Math.max(0, shape.w / 2)
  const ry = Math.max(0, shape.h / 2)
  const points = []
  for (let i = 0; i < sides; i++) {
    const angle = ((360 / sides) * i + rotationDeg) * Math.PI / 180
    points.push(`${cx + Math.cos(angle) * rx},${cy + Math.sin(angle) * ry}`)
  }
  return points.join(' ')
}

function polygonPoints(shape) {
  if (shape.kind === 'triangle') {
    return `${shape.x + shape.w / 2},${shape.y} ${shape.x + shape.w},${shape.y + shape.h} ${shape.x},${shape.y + shape.h}`
  }
  if (shape.kind === 'right-triangle') {
    return `${shape.x},${shape.y} ${shape.x + shape.w},${shape.y + shape.h} ${shape.x},${shape.y + shape.h}`
  }
  if (shape.kind === 'trapezoid') {
    return `${shape.x + shape.w * 0.22},${shape.y} ${shape.x + shape.w * 0.78},${shape.y} ${shape.x + shape.w},${shape.y + shape.h} ${shape.x},${shape.y + shape.h}`
  }
  if (shape.kind === 'parallelogram') {
    return `${shape.x + shape.w * 0.24},${shape.y} ${shape.x + shape.w},${shape.y} ${shape.x + shape.w * 0.76},${shape.y + shape.h} ${shape.x},${shape.y + shape.h}`
  }
  if (shape.kind === 'diamond') {
    return `${shape.x + shape.w / 2},${shape.y} ${shape.x + shape.w},${shape.y + shape.h / 2} ${shape.x + shape.w / 2},${shape.y + shape.h} ${shape.x},${shape.y + shape.h / 2}`
  }
  if (shape.kind === 'chevron') {
    return `${shape.x},${shape.y} ${shape.x + shape.w * 0.72},${shape.y} ${shape.x + shape.w},${shape.y + shape.h / 2} ${shape.x + shape.w * 0.72},${shape.y + shape.h} ${shape.x},${shape.y + shape.h} ${shape.x + shape.w * 0.28},${shape.y + shape.h / 2}`
  }
  if (shape.kind === 'star') return starPoints(shape)
  if (shape.kind === 'pentagon') return regularPolygonPoints(shape, 5, -90)
  if (shape.kind === 'octagon') return regularPolygonPoints(shape, 8, 22.5)
  return regularPolygonPoints(shape, 6, 30)
}

function starPoints(shape) {
  const cx = shape.x + shape.w / 2
  const cy = shape.y + shape.h / 2
  const outerX = Math.max(0, shape.w / 2)
  const outerY = Math.max(0, shape.h / 2)
  const innerX = outerX * 0.42
  const innerY = outerY * 0.42
  const points = []
  for (let i = 0; i < 10; i++) {
    const angle = (-90 + i * 36) * Math.PI / 180
    const rx = i % 2 === 0 ? outerX : innerX
    const ry = i % 2 === 0 ? outerY : innerY
    points.push(`${cx + Math.cos(angle) * rx},${cy + Math.sin(angle) * ry}`)
  }
  return points.join(' ')
}

function pathShapeFill(shape) {
  return filledPathShapeKinds.includes(shape.kind) ? shapeFill(shape) : 'none'
}

function ellipsePath(cx, cy, rx, ry) {
  return `M${cx - rx} ${cy}A${rx} ${ry} 0 1 0 ${cx + rx} ${cy}A${rx} ${ry} 0 1 0 ${cx - rx} ${cy}Z`
}

function pointsPath(points) {
  return points.map((point, index) => `${index === 0 ? 'M' : 'L'}${point[0]} ${point[1]}`).join('') + 'Z'
}

function threeDShapeParts(shape, fillColor = shapeFill(shape)) {
  const x = Number(shape.x) || 0
  const y = Number(shape.y) || 0
  const w = Math.max(0, Number(shape.w) || 0)
  const h = Math.max(0, Number(shape.h) || 0)
  const depth = Math.max(8, Math.min(w, h) * 0.22)
  const fill = fillColor || '#dbe4ea'
  if (shape.kind === 'cylinder') {
    const rx = w / 2
    const ry = Math.max(2, Math.min(h * 0.12, 18))
    const cx = x + rx
    const topY = y + ry
    const bottomY = y + h - ry
    return [
      { key: 'body', d: `M${x} ${topY}L${x} ${bottomY}A${rx} ${ry} 0 0 0 ${x + w} ${bottomY}L${x + w} ${topY}A${rx} ${ry} 0 0 1 ${x} ${topY}Z`, fill, fillOpacity: 0.86 },
      { key: 'top', d: ellipsePath(cx, topY, rx, ry), fill: '#ffffff', fillOpacity: 0.36 },
      { key: 'bottom', d: `M${x} ${bottomY}A${rx} ${ry} 0 0 0 ${x + w} ${bottomY}`, fill: 'none', strokeOpacity: 0.65 }
    ]
  }
  if (shape.kind === 'cylinder-h') {
    const rx = Math.max(2, Math.min(w * 0.12, 18))
    const ry = h / 2
    const cy = y + ry
    const leftX = x + rx
    const rightX = x + w - rx
    return [
      { key: 'body', d: `M${leftX} ${y}H${rightX}A${rx} ${ry} 0 0 1 ${rightX} ${y + h}H${leftX}A${rx} ${ry} 0 0 0 ${leftX} ${y}Z`, fill, fillOpacity: 0.86 },
      { key: 'left', d: ellipsePath(leftX, cy, rx, ry), fill: '#ffffff', fillOpacity: 0.28 },
      { key: 'right', d: ellipsePath(rightX, cy, rx, ry), fill: '#ffffff', fillOpacity: 0.18 }
    ]
  }
  if (shape.kind === 'cube') {
    const d = Math.min(depth, w * 0.34, h * 0.34)
    return [
      { key: 'front', d: pointsPath([[x, y + d], [x + w - d, y + d], [x + w - d, y + h], [x, y + h]]), fill, fillOpacity: 0.9 },
      { key: 'top', d: pointsPath([[x, y + d], [x + d, y], [x + w, y], [x + w - d, y + d]]), fill: '#ffffff', fillOpacity: 0.32 },
      { key: 'side', d: pointsPath([[x + w - d, y + d], [x + w, y], [x + w, y + h - d], [x + w - d, y + h]]), fill: '#0f172a', fillOpacity: 0.12 }
    ]
  }
  if (shape.kind === 'cone') {
    const rx = w / 2
    const ry = Math.max(2, Math.min(h * 0.1, 16))
    const cx = x + rx
    const baseY = y + h - ry
    return [
      { key: 'body', d: `M${cx} ${y}L${x} ${baseY}A${rx} ${ry} 0 0 0 ${x + w} ${baseY}Z`, fill, fillOpacity: 0.88 },
      { key: 'base', d: ellipsePath(cx, baseY, rx, ry), fill: '#ffffff', fillOpacity: 0.25 }
    ]
  }
  if (shape.kind === 'frustum') {
    const topW = w * 0.7
    const bottomW = w * 0.34
    const topX = x + (w - topW) / 2
    const bottomX = x + (w - bottomW) / 2
    const topRy = Math.max(2, Math.min(h * 0.09, 13))
    const bottomRy = Math.max(2, Math.min(h * 0.07, 10))
    const topY = y + topRy
    const bottomY = y + h - bottomRy
    return [
      { key: 'body', d: pointsPath([[topX, topY], [topX + topW, topY], [bottomX + bottomW, bottomY], [bottomX, bottomY]]), fill, fillOpacity: 0.88 },
      { key: 'top', d: ellipsePath(x + w / 2, topY, topW / 2, topRy), fill: '#ffffff', fillOpacity: 0.32 },
      { key: 'bottom', d: ellipsePath(x + w / 2, bottomY, bottomW / 2, bottomRy), fill: '#ffffff', fillOpacity: 0.2 }
    ]
  }
  const d = Math.min(depth, w * 0.28, h * 0.28)
  return [
    { key: 'front', d: pointsPath([[x, y + h], [x + w * 0.5, y + d], [x + w - d, y + h]]), fill, fillOpacity: 0.9 },
    { key: 'top', d: pointsPath([[x + w * 0.5, y + d], [x + w * 0.5 + d, y], [x + w, y + h - d], [x + w - d, y + h]]), fill: '#ffffff', fillOpacity: 0.28 },
    { key: 'side', d: pointsPath([[x + w - d, y + h], [x + w, y + h - d], [x + d, y + h - d], [x, y + h]]), fill: '#0f172a', fillOpacity: 0.1 }
  ]
}

function shapePath(shape) {
  const x = Number(shape.x) || 0
  const y = Number(shape.y) || 0
  const w = Math.max(0, Number.isFinite(Number(shape.w)) ? Number(shape.w) : 0)
  const h = Math.max(0, Number.isFinite(Number(shape.h)) ? Number(shape.h) : 0)
  if (shape.kind === 'arrow-left') {
    return `M${x + w} ${y + h * 0.25}H${x + w * 0.35}V${y}L${x} ${y + h * 0.5}L${x + w * 0.35} ${y + h}V${y + h * 0.75}H${x + w}Z`
  }
  if (shape.kind === 'arc') {
    return `M${x} ${y + h}C${x + w * 0.25} ${y},${x + w * 0.75} ${y},${x + w} ${y + h}`
  }
  if (shape.kind === 'pipe-elbow') {
    return `M${x + w * 0.18} ${y + h}V${y + h * 0.38}Q${x + w * 0.18} ${y + h * 0.18},${x + w * 0.38} ${y + h * 0.18}H${x + w}`
  }
  if (shape.kind === 'pipe-tee') {
    return `M${x} ${y + h * 0.28}H${x + w}M${x + w * 0.5} ${y + h * 0.28}V${y + h}`
  }
  return `M${x} ${y + h * 0.25}H${x + w * 0.65}V${y}L${x + w} ${y + h * 0.5}L${x + w * 0.65} ${y + h}V${y + h * 0.75}H${x}Z`
}

function shapeRotation(shape) {
  return Number.isFinite(Number(shape.rotate)) ? Number(shape.rotate) : 0
}

function shapeTransform(shape) {
  const rotate = shapeRotation(shape)
  if (!rotate) return ''
  const x = Number(shape.x) || 0
  const y = Number(shape.y) || 0
  const w = Number(shape.w) || 0
  const h = Number(shape.h) || 0
  return `rotate(${rotate} ${x + w / 2} ${y + h / 2})`
}

const label = computed(() => props.widget.config?.label || props.widget.config?.title || '')
const hideName = computed(() => Boolean(props.widget.config?.hideName))
const labelStyle = computed(() => ({
  transform: `translate(-50%, 0) translate(${Number(props.widget.config?.labelOffsetX) || 0}px, ${Number(props.widget.config?.labelOffsetY) || 0}px)`,
  fontSize: `${Math.max(8, Math.min(72, Number(props.widget.config?.labelFontSize) || 12))}px`
}))
</script>

<style scoped>
.custom-shape-widget {
  position: relative;
  width: 100%;
  height: 100%;
  overflow: visible;
}
.custom-shape-svg {
  width: 100%;
  height: 100%;
  display: block;
  overflow: hidden;
  shape-rendering: geometricPrecision;
}
.custom-shape-label {
  position: absolute;
  left: 50%;
  top: -30px;
  z-index: 4;
  max-width: 160%;
  padding: 2px 7px;
  border-radius: var(--radius-sm);
  background: rgba(255, 255, 255, 0.82);
  border: 1px solid rgba(120, 133, 146, 0.28);
  color: var(--text-primary);
  font-weight: 700;
  white-space: nowrap;
}
</style>
