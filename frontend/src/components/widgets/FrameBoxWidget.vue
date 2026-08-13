<template>
  <div class="frame-box-widget" :style="frameStyle">
    <div v-if="title && !hideName" class="frame-box-title" :style="titleStyle">{{ title }}</div>
  </div>
</template>

<script setup>
import { computed } from 'vue'

const props = defineProps({
  widget: { type: Object, required: true }
})

function clamp(value, min, max, fallback) {
  const number = Number(value)
  return Math.max(min, Math.min(max, Number.isFinite(number) ? number : fallback))
}

function colorWithOpacity(color, opacity) {
  const text = String(color || '').trim()
  const hex = text.match(/^#?([0-9a-f]{6})$/i)?.[1]
  if (!hex) return text || 'transparent'
  const r = parseInt(hex.slice(0, 2), 16)
  const g = parseInt(hex.slice(2, 4), 16)
  const b = parseInt(hex.slice(4, 6), 16)
  return `rgba(${r}, ${g}, ${b}, ${opacity})`
}

const frameStyle = computed(() => {
  const config = props.widget.config || {}
  const opacity = clamp(config.backgroundOpacity, 0, 100, 0) / 100
  const borderWidth = clamp(config.borderWidth, 0, 40, 2)
  const borderRadius = clamp(config.borderRadius, 0, 120, 8)
  const borderStyle = ['solid', 'dashed', 'dotted'].includes(config.borderStyle)
    ? config.borderStyle
    : 'solid'

  return {
    background: colorWithOpacity(config.background || '#ffffff', opacity),
    borderColor: config.borderColor || '#4f6fb8',
    borderWidth: `${borderWidth}px`,
    borderRadius: `${borderRadius}px`,
    borderStyle
  }
})

const title = computed(() => props.widget.config?.title || '边框容器')
const hideName = computed(() => Boolean(props.widget.config?.hideName))
const titleStyle = computed(() => ({
  fontSize: `${clamp(props.widget.config?.titleFontSize, 8, 72, 12)}px`
}))
</script>

<style scoped>
.frame-box-widget {
  width: 100%;
  height: 100%;
  box-sizing: border-box;
  pointer-events: none;
  position: relative;
}

.frame-box-title {
  position: absolute;
  left: 12px;
  top: 0;
  max-width: calc(100% - 24px);
  transform: translateY(-50%);
  padding: 1px 6px;
  border-radius: 4px;
  background: rgba(255, 255, 255, 0.92);
  color: var(--text-secondary);
  font-weight: 700;
  line-height: 1.2;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
  pointer-events: none;
}
</style>
