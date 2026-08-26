<template>
  <div class="scada-svg-widget" :class="{ active: isActive }" :style="shellStyle">
    <svg v-if="asset?.type === 'geometry' && asset.path" class="svg-geometry" viewBox="0 0 80 80" :preserveAspectRatio="preserveAspectRatio">
      <path :d="asset.path" :style="geometryStyle" />
    </svg>
    <img v-else-if="asset?.path || asset?.url" class="svg-image" :src="asset.path || asset.url" :alt="label" :style="imageStyle" draggable="false" />
    <div v-else class="svg-empty">{{ label || '导入 SVG/图片' }}</div>
    <span v-if="!config.hideName && label" class="svg-label" :style="labelStyle">{{ label }}</span>
  </div>
</template>

<script setup>
import { computed } from 'vue'
import { useMonitorStore } from '../../store/index.js'

const props = defineProps({ widget: { type: Object, required: true } })
const store = useMonitorStore()
const config = computed(() => props.widget?.config || {})
const asset = computed(() => config.value.asset || null)
const label = computed(() => config.value.label || config.value.title || asset.value?.name || '')
const boundValue = computed(() => config.value.bindVariable ? store.realtimeData[config.value.varId]?.value : null)
const isActive = computed(() => {
  if (!config.value.bindVariable || config.value.varId == null) return false
  const numeric = Number(boundValue.value)
  return Number.isFinite(numeric) ? numeric !== 0 : Boolean(boundValue.value)
})
const clamp = (value, min, max, fallback) => Math.max(min, Math.min(max, Number.isFinite(Number(value)) ? Number(value) : fallback))
const color = (value, fallback) => /^#[0-9a-f]{3,8}$/i.test(String(value || '')) ? value : fallback
const rgba = (hex, opacity) => {
  const raw = color(hex, '#000000').slice(1)
  const normalized = raw.length === 3 ? raw.split('').map(item => item + item).join('') : raw.slice(0, 6)
  const value = Number.parseInt(normalized, 16)
  return `rgba(${(value >> 16) & 255}, ${(value >> 8) & 255}, ${value & 255}, ${opacity})`
}
const stateColor = computed(() => isActive.value ? color(config.value.statusOnColor, '#22c55e') : color(config.value.statusOffColor, '#64748b'))
const shellStyle = computed(() => {
  const borderWidth = clamp(config.value.borderWidth, 0, 24, 0)
  const activeBorder = config.value.bindVariable && borderWidth === 0 ? 1 : borderWidth
  return {
    background: rgba(color(config.value.background, '#ffffff'), clamp(config.value.backgroundOpacity, 0, 100, 0) / 100),
    border: `${activeBorder}px solid ${config.value.bindVariable ? stateColor.value : color(config.value.borderColor, '#4f6fb8')}`,
    borderRadius: `${clamp(config.value.borderRadius, 0, 80, 0)}px`,
    boxShadow: config.value.bindVariable ? `0 0 0 2px ${rgba(stateColor.value, 0.15)}` : undefined
  }
})
const preserveAspectRatio = computed(() => config.value.preserveAspectRatio === 'slice' ? 'xMidYMid slice' : config.value.preserveAspectRatio === 'none' ? 'none' : 'xMidYMid meet')
const imageStyle = computed(() => ({
  opacity: clamp(config.value.opacity, 0, 100, 100) / 100,
  objectFit: config.value.preserveAspectRatio === 'slice' ? 'cover' : config.value.preserveAspectRatio === 'none' ? 'fill' : 'contain'
}))
const geometryStyle = computed(() => ({
  fill: config.value.bindVariable ? rgba(stateColor.value, 0.38) : color(config.value.fillColor, '#0ea5e9'),
  stroke: config.value.bindVariable ? stateColor.value : color(config.value.strokeColor, '#0284c7'),
  strokeWidth: clamp(config.value.strokeWidth, 0, 24, 2)
}))
const labelStyle = computed(() => ({
  transform: `translate(-50%, 0) translate(${Number(config.value.labelOffsetX) || 0}px, ${Number(config.value.labelOffsetY) || 0}px)`,
  fontSize: `${clamp(config.value.labelFontSize, 8, 72, 12)}px`
}))
</script>

<style scoped>
.scada-svg-widget { position: relative; display: flex; align-items: stretch; width: 100%; height: 100%; box-sizing: border-box; overflow: visible; }
.svg-image, .svg-geometry { display: block; width: 100%; height: 100%; user-select: none; pointer-events: none; }
.svg-empty { display: flex; align-items: center; justify-content: center; width: 100%; height: 100%; border: 1px dashed var(--border-color); color: var(--text-muted); font-size: var(--fs-sm); text-align: center; }
.svg-label { position: absolute; left: 50%; bottom: 2px; max-width: calc(100% - 8px); padding: 2px 6px; overflow: hidden; border: 1px solid var(--widget-label-border); border-radius: var(--radius-sm); background: var(--widget-label-bg); box-shadow: var(--widget-label-shadow); color: var(--widget-label-text); font-weight: var(--fw-semibold); line-height: 1.25; text-align: center; text-overflow: ellipsis; white-space: nowrap; }
</style>
