<template>
  <div class="iw" :class="stateClass">
    <div class="iw-dot"></div>
    <div v-if="!hideName" class="iw-label">{{ widget.config.title || '状态' }}</div>
    <div class="iw-text">{{ stateText }}</div>
  </div>
</template>

<script setup>
import { computed } from 'vue'
import { useMonitorStore } from '../../store/index.js'
const props = defineProps({ widget: { type: Object, required: true } })
const store = useMonitorStore()
const varId = computed(() => props.widget.config.varId)
const hideName = computed(() => Boolean(props.widget.config?.hideName))
const threshold = computed(() => props.widget.config.threshold ?? 50)
const dp = computed(() => (varId.value != null) ? store.realtimeData[varId.value] : null)
const curVal = computed(() => dp.value?.value ?? 0)
const stateClass = computed(() => {
  if (!dp.value || dp.value.quality !== 'GOOD') return 'offline'
  return curVal.value > threshold.value ? 'alert' : 'normal'
})
const stateText = computed(() => stateClass.value === 'normal' ? '正常' : stateClass.value === 'alert' ? '报警' : '离线')
</script>

<style scoped>
.iw {
  display: flex; flex-direction: column; align-items: center;
  justify-content: center; height: 100%; gap: 4px;
  padding: 8px;
}
.iw-dot {
  width: 22px; height: 22px; border-radius: 50%;
  transition: all 0.3s ease;
}
.normal .iw-dot { background: var(--color-online); box-shadow: 0 0 0 6px rgba(47,143,99,0.12); }
.alert .iw-dot { background: var(--color-offline); box-shadow: 0 0 0 6px rgba(194,65,75,0.12); animation: fadeIn 0.6s ease infinite alternate; }
.offline .iw-dot { background: var(--text-placeholder); }
.iw-label { font-size: var(--fs-xs); color: var(--text-secondary); font-weight: var(--fw-semibold); }
.iw-text { font-size: 10px; color: var(--text-placeholder); font-family: var(--font-mono); }
</style>
