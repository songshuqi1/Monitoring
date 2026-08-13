<template>
  <div class="status-circle-widget" :class="stateClass">
    <svg class="status-circle-svg" viewBox="0 0 100 100" preserveAspectRatio="xMidYMid meet" aria-hidden="true">
      <circle class="status-circle-halo" cx="50" cy="50" r="45" />
      <circle class="status-circle" cx="50" cy="50" r="42" />
    </svg>
  </div>
</template>

<script setup>
import { computed } from 'vue'
import { useMonitorStore } from '../../store/index.js'

const props = defineProps({
  widget: { type: Object, required: true }
})

const store = useMonitorStore()
const varId = computed(() => props.widget.config?.varId)
const threshold = computed(() => Number(props.widget.config?.threshold ?? 0))
const dp = computed(() => (varId.value != null) ? store.realtimeData[varId.value] : null)

function activeFromValue(value) {
  if (typeof value === 'boolean') return value
  const text = String(value ?? '').trim().toLowerCase()
  if (['true', 'on', 'open', 'run', 'running', 'start', 'yes', '开', '开启', '打开', '运行', '启动'].includes(text)) return true
  if (['false', 'off', 'close', 'closed', 'stop', 'stopped', 'no', '关', '关闭', '停止'].includes(text)) return false
  const number = Number(value)
  return Number.isFinite(number) && number > threshold.value
}

const stateClass = computed(() => {
  if (!dp.value || dp.value.quality !== 'GOOD') return 'state-off'
  return activeFromValue(dp.value.value) ? 'state-on' : 'state-off'
})
</script>

<style scoped>
.status-circle-widget {
  width: 100%;
  height: 100%;
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 0;
  box-sizing: border-box;
}

.status-circle-svg {
  width: 100%;
  height: 100%;
  display: block;
  overflow: visible;
}

.status-circle-halo {
  fill: rgba(239, 68, 68, 0.14);
  transition: fill 0.18s ease;
}

.status-circle {
  fill: #ef4444;
  stroke: rgba(127, 29, 29, 0.35);
  stroke-width: 2;
  filter: drop-shadow(0 1px 1px rgba(15, 23, 42, 0.16));
  transition: fill 0.18s ease, stroke 0.18s ease;
}

.state-on .status-circle-halo {
  fill: rgba(34, 197, 94, 0.16);
}

.state-on .status-circle {
  fill: #22c55e;
  stroke: rgba(22, 101, 52, 0.36);
}
</style>
