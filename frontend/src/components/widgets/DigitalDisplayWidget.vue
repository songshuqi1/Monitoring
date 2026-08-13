<template>
  <div class="digital-display">
    <div class="dd-value" :style="{ color: valueColor }">{{ displayValue }}</div>
    <div class="dd-unit">{{ widget.config.unit || '' }}</div>
    <div class="dd-quality">
      <span class="quality-dot" :class="qualityClass"></span>
      {{ qualityText }}
    </div>
  </div>
</template>

<script setup>
import { computed } from 'vue'
import { useMonitorStore } from '../../store/index.js'

const props = defineProps({ widget: { type: Object, required: true } })
const store = useMonitorStore()
const varId = computed(() => props.widget.config.varId)
const dp = computed(() => (varId.value != null) ? store.realtimeData[varId.value] : null)
const numericValue = computed(() => {
  if (!dp.value) return null
  const num = Number(dp.value.value)
  return Number.isFinite(num) ? num : null
})
const displayValue = computed(() => {
  if (!dp.value) return '---'
  if (numericValue.value !== null) return numericValue.value.toFixed(props.widget.config.decimals ?? 1)
  return dp.value.value === undefined || dp.value.value === null ? '---' : String(dp.value.value)
})
const valueColor = computed(() => {
  if (!dp.value) return 'var(--text-placeholder)'
  if (dp.value.quality !== 'GOOD') return 'var(--color-offline)'
  const { min, max } = props.widget.config
  if (numericValue.value !== null && min != null && numericValue.value < min) return 'var(--color-warning)'
  if (numericValue.value !== null && max != null && numericValue.value > max) return 'var(--color-alarm)'
  return 'var(--accent)'
})
const qualityClass = computed(() => !dp.value ? 'bad' : (dp.value.quality || '').toLowerCase())
const qualityText = computed(() => !dp.value ? '离线' : dp.value.quality || '未知')
</script>

<style scoped>
.digital-display {
  display: flex; flex-direction: column; align-items: center;
  justify-content: center; height: 100%; padding: 12px;
  background: linear-gradient(180deg, rgba(255,255,255,0.82), rgba(248,250,252,0.82));
}
.dd-value {
  font-family: var(--font-mono);
  font-size: var(--fs-3xl);
  font-weight: var(--fw-bold);
  line-height: 1.1;
  color: var(--accent);
}
.dd-unit { font-size: var(--fs-sm); color: var(--text-tertiary); margin-top: 4px; }
.dd-quality { display: flex; align-items: center; gap: 6px; margin-top: 8px; font-size: var(--fs-xs); color: var(--text-placeholder); }
</style>
