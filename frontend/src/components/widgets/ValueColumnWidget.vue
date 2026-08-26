<template>
  <div class="value-column-widget">
    <div class="value-column-body" :class="`value-${valuePosition}`">
      <div class="value-column-info" :style="{ color: textColor }">
        <span class="value-column-value">{{ displayValue }}</span>
        <span class="value-column-unit" :style="{ color: scaleColor }">{{ config.unit || '' }}</span>
      </div>
      <div class="value-column-scale" :style="{ color: scaleColor }">
        <span class="scale-label scale-top">{{ maxLabel }}</span>
        <span class="scale-label scale-mid">{{ midLabel }}</span>
        <span class="scale-label scale-bottom">{{ minLabel }}</span>
      </div>
      <div class="value-column-track" :style="{ background: trackColor }">
        <span v-for="tick in tickPercents" :key="tick" class="value-column-tick" :style="{ bottom: `${tick}%`, background: scaleColor }"></span>
        <div class="value-column-fill" :style="fillStyle"></div>
      </div>
    </div>
    <div class="value-column-quality" :class="qualityClass" :title="qualityText"></div>
  </div>
</template>

<script setup>
import { computed } from 'vue'
import { useMonitorStore } from '../../store/index.js'

const props = defineProps({ widget: { type: Object, required: true } })
const store = useMonitorStore()
const config = computed(() => props.widget?.config || {})
const dataPoint = computed(() => config.value.varId != null ? store.realtimeData[config.value.varId] : null)
const value = computed(() => {
  const number = Number(dataPoint.value?.value)
  return Number.isFinite(number) ? number : null
})
const min = computed(() => Number.isFinite(Number(config.value.min)) ? Number(config.value.min) : 0)
const max = computed(() => Number.isFinite(Number(config.value.max)) ? Number(config.value.max) : 100)
const decimals = computed(() => Math.max(0, Math.min(6, Math.trunc(Number(config.value.decimals) || 0))))
const valuePosition = computed(() => config.value.valuePosition === 'left' ? 'left' : 'right')
const barColor = computed(() => config.value.barColor || '#ec4899')
const trackColor = computed(() => config.value.trackColor || '#111827')
const scaleColor = computed(() => config.value.scaleColor || '#98a2b3')
const textColor = computed(() => config.value.textColor || '#f8fafc')
const tickPercents = [0, 20, 40, 60, 80, 100]
const range = computed(() => Math.abs(max.value - min.value) < 1e-9 ? 1 : max.value - min.value)
const fillPercent = computed(() => value.value == null ? 0 : Math.max(0, Math.min(100, (value.value - min.value) / range.value * 100)))
const fillStyle = computed(() => ({ height: `${fillPercent.value}%`, background: `linear-gradient(180deg, ${barColor.value}, ${barColor.value}cc)` }))
const format = (number) => Number.isFinite(Number(number)) ? Number(number).toFixed(decimals.value) : '--'
const displayValue = computed(() => value.value == null ? '---' : format(value.value))
const minLabel = computed(() => format(min.value))
const maxLabel = computed(() => format(max.value))
const midLabel = computed(() => format((min.value + max.value) / 2))
const qualityClass = computed(() => String(dataPoint.value?.quality || '').toUpperCase() === 'GOOD' ? 'good' : 'bad')
const qualityText = computed(() => dataPoint.value?.quality || '离线')
</script>

<style scoped>
.value-column-widget { width: 100%; height: 100%; position: relative; display: flex; align-items: stretch; justify-content: center; padding: 6px 8px; box-sizing: border-box; overflow: hidden; background: linear-gradient(180deg, rgba(15,23,42,.05), rgba(15,23,42,.01)); }
.value-column-body { display: grid; grid-template-columns: minmax(26px,28%) minmax(22px,1fr) auto; align-items: stretch; gap: 8px; width: 100%; height: 100%; min-height: 0; }
.value-column-body.value-left { grid-template-columns: auto minmax(22px,1fr) minmax(26px,28%); }
.value-column-body.value-right .value-column-scale { order: 1; }
.value-column-body.value-right .value-column-track { order: 2; }
.value-column-body.value-right .value-column-info { order: 3; align-items: flex-start; text-align: left; }
.value-column-info { display: flex; flex-direction: column; justify-content: flex-end; align-items: flex-end; gap: 2px; min-width: 0; padding-bottom: 2px; text-align: right; }
.value-column-value { font-family: var(--font-mono); font-size: 22px; font-weight: var(--fw-bold); line-height: 1; }
.value-column-unit { font-size: 11px; font-weight: 700; line-height: 1.15; white-space: nowrap; }
.value-column-scale { position: relative; min-height: 0; font-family: var(--font-mono); font-size: 11px; font-weight: 700; }
.scale-label { position: absolute; left: 0; transform: translateY(50%); white-space: nowrap; }
.scale-top { top: 0; transform: translateY(-50%); }.scale-mid { top: 50%; transform: translateY(-50%); }.scale-bottom { bottom: 0; transform: translateY(50%); }
.value-column-track { position: relative; min-height: 0; overflow: hidden; box-shadow: inset 0 0 0 1px rgba(255,255,255,.14); }
.value-column-tick { position: absolute; left: 0; width: 58%; height: 1px; opacity: .9; }
.value-column-fill { position: absolute; left: 0; right: 0; bottom: 0; box-shadow: inset 0 0 0 1px rgba(255,255,255,.16); transition: height .25s ease; }
.value-column-quality { position: absolute; right: 6px; top: 6px; width: 8px; height: 8px; border-radius: 999px; box-shadow: 0 0 0 2px rgba(255,255,255,.08); }.value-column-quality.good { background: #16a34a; }.value-column-quality.bad { background: #ef4444; }
</style>
