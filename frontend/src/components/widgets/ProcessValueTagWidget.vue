<template>
  <div class="value-tag" :class="qualityClass">
    <div v-if="label && !hideName" class="tag-label" :style="labelStyle">{{ label }}</div>
    <div class="tag-main">
      <span class="tag-value" :style="valueStyle">{{ displayValue }}</span>
      <span class="tag-unit" :style="unitStyle">{{ displayUnit }}</span>
    </div>
  </div>
</template>

<script setup>
import { computed } from 'vue'
import { useMonitorStore } from '../../store/index.js'

const props = defineProps({ widget: { type: Object, required: true } })
const store = useMonitorStore()

const label = computed(() => props.widget.config.label || '')
const hideName = computed(() => Boolean(props.widget.config?.hideName))
const varId = computed(() => props.widget.config.varId)
const dp = computed(() => (varId.value != null) ? store.realtimeData[varId.value] : null)
const displayValue = computed(() => {
  if (!dp.value || typeof Number(dp.value.value) !== 'number') return '0.0'
  const value = Number(dp.value.value)
  if (!Number.isFinite(value)) return '0.0'
  return value.toFixed(props.widget.config.decimals ?? 1)
})
const displayUnit = computed(() => props.widget.config.unit || '')
const titleFontSize = computed(() => Math.max(8, Math.min(72, Number(props.widget.config.titleFontSize) || 11)))
const valueFontSize = computed(() => Math.max(8, Math.min(72, Number(props.widget.config.valueFontSize) || 16)))
const labelStyle = computed(() => ({ fontSize: `${titleFontSize.value}px` }))
const valueStyle = computed(() => ({ fontSize: `${valueFontSize.value}px` }))
const unitStyle = computed(() => ({ fontSize: `${Math.max(8, Math.round(valueFontSize.value * 0.72))}px` }))
const qualityClass = computed(() => {
  if (!dp.value) return 'offline'
  return dp.value.quality === 'GOOD' ? 'good' : 'offline'
})
</script>

<style scoped>
.value-tag {
  width: 100%;
  height: 100%;
  display: flex;
  flex-direction: column;
  justify-content: center;
  gap: 2px;
  padding: 0;
  border: 0;
  background: transparent;
  box-shadow: none;
  font-family: var(--font-mono);
  overflow: visible;
  user-select: none;
}
.tag-label {
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
  color: var(--text-secondary);
  font-size: 11px;
  font-family: var(--font-sans);
  font-weight: 700;
  text-align: center;
  line-height: 1.15;
}
.tag-main {
  display: flex;
  align-items: baseline;
  justify-content: center;
  gap: 4px;
  min-width: 0;
}
.tag-value {
  color: #665c0a;
  font-size: 16px;
  font-weight: 800;
  line-height: 1.05;
  text-shadow: 0 1px 0 rgba(255, 255, 255, 0.6);
}
.tag-unit {
  color: var(--text-secondary);
  font-size: 12px;
  white-space: nowrap;
}
.good {
  opacity: 1;
}
.offline {
  opacity: 0.86;
}
</style>
