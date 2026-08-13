<template>
  <div class="alw">
    <div class="alw-scroll">
      <div v-for="alarm in store.alarms.slice(0, 30)" :key="alarm.id" class="alw-row" :class="{ unack: !alarm.acknowledged }">
        <span class="alw-dot" :style="{ background: sv(alarm.severity) }"></span>
        <span class="alw-var">{{ alarm.varName }}</span>
        <span class="alw-msg">{{ alarm.message }}</span>
        <span class="badge" :class="alarm.alarmType === 'HH' || alarm.alarmType === 'LL' ? 'badge-err' : 'badge-warn'">{{ alarm.alarmType }}</span>
      </div>
      <div v-if="store.alarms.length === 0" class="empty-state" style="height:100%">暂无报警</div>
    </div>
  </div>
</template>

<script setup>
import { useMonitorStore } from '../../store/index.js'
defineProps({ widget: { type: Object, required: true } })
const store = useMonitorStore()
function sv(s) { return ['#98a2b3','#a66a1f','#a66a1f','#c2414b','#c2414b'][Math.min(s,5)-1] || '#98a2b3' }
</script>

<style scoped>
.alw { height: 100%; overflow: hidden; }
.alw-scroll { height: 100%; overflow-y: auto; }
.alw-row {
  display: flex; align-items: center; gap: 8px;
  min-height: 34px;
  padding: 6px 10px; font-size: var(--fs-xs);
  border-bottom: 1px solid var(--border-light);
  animation: fadeIn 0.2s ease;
}
.alw-row:hover { background: var(--bg-hover); }
.alw-row.unack { background: var(--danger-bg); }
.alw-dot { width: 6px; height: 6px; border-radius: 50%; flex-shrink: 0; }
.alw-var { color: var(--text-secondary); min-width: 50px; font-weight: var(--fw-semibold); }
.alw-msg { flex: 1; color: var(--text-primary); overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
</style>
