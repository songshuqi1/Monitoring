<template>
  <div class="bottom-dock" :style="{ height: `${height}px` }">
    <div class="bottom-dock-resizer" title="拖动调整底部面板高度" @mousedown.prevent="startResize">
      <span></span>
    </div>
    <div class="dock-tabs">
      <button v-for="tab in tabs" :key="tab.key"
        :class="['dock-tab', { active: activeTab === tab.key }]"
        @click="activeTab = tab.key">
        {{ tab.label }}
        <span v-if="tab.badge" class="dock-badge">{{ tab.badge }}</span>
      </button>
    </div>

    <div class="dock-content">
      <!-- 报警列表 -->
      <div v-show="activeTab === 'alarms'" class="dock-scroll">
        <div v-if="store.alarms.length === 0" class="dock-empty">暂无报警记录</div>
        <div v-for="alarm in store.alarms.slice(0, 50)" :key="alarm.id" class="dock-row" :class="{ unack: !alarm.acknowledged }">
          <span class="dr-dot" :style="{ background: severityColor(alarm.severity) }"></span>
          <span class="dr-time">{{ formatTime(alarm.occurredAtMs) }}</span>
          <span class="dr-var">{{ alarm.varName }}</span>
          <span class="dr-type badge" :class="alarm.alarmType === 'HH' || alarm.alarmType === 'LL' ? 'badge-err' : 'badge-warn'">{{ alarm.alarmType }}</span>
          <span class="dr-msg">{{ alarm.message }}</span>
          <span class="dr-val">{{ formatValue(alarm.alarmValue, 2) }}</span>
        </div>
      </div>

      <!-- 操作日志 -->
      <div v-show="activeTab === 'logs'" class="dock-scroll">
        <div v-if="store.operationLogs.length === 0" class="dock-empty">暂无操作日志</div>
        <div v-for="log in store.operationLogs.slice(0, 50)" :key="log.id" class="dock-row">
          <span class="dr-time">{{ formatTime(log.operatedAtMs) }}</span>
          <span class="dr-type-badge" :class="log.operationType">{{ log.operationType }}</span>
          <span class="dr-var">{{ log.varName }}</span>
          <span class="dr-msg">{{ log.detail || log.operator_ }}</span>
        </div>
      </div>

      <!-- 变量列表 -->
      <div v-show="activeTab === 'variables'" class="dock-scroll">
        <div v-if="store.variables.length === 0" class="dock-empty">暂无变量定义</div>
        <div v-for="v in store.variables" :key="v.id" class="dock-row">
          <span class="quality-dot" :class="getQuality(v.id)"></span>
          <span class="dr-var" style="min-width:80px">{{ v.name }}</span>
          <span class="dr-val" style="min-width:60px">{{ getDisplayValue(v.id) }}</span>
          <span class="dr-unit" style="min-width:30px">{{ v.unit }}</span>
          <span class="dr-source" style="min-width:50px">{{ v.source }}</span>
        </div>
      </div>

      <!-- 画布总览 -->
      <div v-show="activeTab === 'overview'" class="dock-overview">
        <div
          ref="overviewMapRef"
          class="overview-map"
          :class="{ dragging: overviewDragging }"
          @mousedown="startOverviewDrag"
        >
          <div v-if="store.widgets.length === 0" class="overview-empty">暂无画布组件</div>
          <div
            v-for="item in overviewWidgets"
            :key="item.id"
            class="overview-widget"
            :style="item.style"
            :title="item.title"
          ></div>
          <div class="overview-viewport" :style="overviewViewportStyle"></div>
        </div>
        <div class="overview-side">
          <div class="overview-title">画布视图</div>
          <div class="overview-meta">{{ store.widgets.length }} 个组件</div>
          <div class="overview-meta">拖动或点击定位</div>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, computed, onMounted, onUnmounted } from 'vue'
import { useMonitorStore } from '../store/index.js'

const props = defineProps({
  height: {
    type: Number,
    default: 180
  },
  viewport: {
    type: Object,
    default: () => ({
      panX: 0,
      panY: 0,
      zoom: 1,
      width: 0,
      height: 0,
      canvasOffset: { x: 0, y: 0 }
    })
  }
})

const emit = defineEmits(['resize-start', 'minimap-navigate'])
const store = useMonitorStore()
const activeTab = ref('alarms')
const overviewMapRef = ref(null)
const overviewDragging = ref(false)

const tabs = computed(() => [
  { key: 'alarms', label: '报警', badge: store.alarms.length },
  { key: 'logs', label: '操作日志' },
  { key: 'variables', label: '变量列表' },
  { key: 'overview', label: '视图' },
])

const currentViewportRect = computed(() => {
  const z = Number(props.viewport.zoom) || 1
  return {
    x: -Number(props.viewport.panX || 0) / z,
    y: -Number(props.viewport.panY || 0) / z,
    w: Math.max(1, Number(props.viewport.width || 0) / z),
    h: Math.max(1, Number(props.viewport.height || 0) / z)
  }
})

const overviewBounds = computed(() => {
  const offset = props.viewport.canvasOffset || { x: 0, y: 0 }
  const visible = currentViewportRect.value
  let minX = visible.x
  let minY = visible.y
  let maxX = visible.x + visible.w
  let maxY = visible.y + visible.h

  store.widgets.forEach(widget => {
    const x = (Number(widget.x) || 0) + (Number(offset.x) || 0)
    const y = (Number(widget.y) || 0) + (Number(offset.y) || 0)
    const w = Math.max(1, Number(widget.w) || 1)
    const h = Math.max(1, Number(widget.h) || 1)
    minX = Math.min(minX, x)
    minY = Math.min(minY, y)
    maxX = Math.max(maxX, x + w)
    maxY = Math.max(maxY, y + h)
  })

  const width = Math.max(1, maxX - minX)
  const height = Math.max(1, maxY - minY)
  const pad = Math.max(160, Math.min(900, Math.max(width, height) * 0.08))
  return {
    minX: minX - pad,
    minY: minY - pad,
    width: width + pad * 2,
    height: height + pad * 2
  }
})

function percentX(x) {
  return ((x - overviewBounds.value.minX) / overviewBounds.value.width) * 100
}

function percentY(y) {
  return ((y - overviewBounds.value.minY) / overviewBounds.value.height) * 100
}

const overviewWidgets = computed(() => {
  const offset = props.viewport.canvasOffset || { x: 0, y: 0 }
  return store.widgets.map(widget => {
    const x = (Number(widget.x) || 0) + (Number(offset.x) || 0)
    const y = (Number(widget.y) || 0) + (Number(offset.y) || 0)
    const w = Math.max(1, Number(widget.w) || 1)
    const h = Math.max(1, Number(widget.h) || 1)
    return {
      id: widget.id,
      title: widget.config?.title || widget.type || widget.id,
      style: {
        left: `${percentX(x)}%`,
        top: `${percentY(y)}%`,
        width: `${Math.max(0.8, (w / overviewBounds.value.width) * 100)}%`,
        height: `${Math.max(0.8, (h / overviewBounds.value.height) * 100)}%`
      }
    }
  })
})

const overviewViewportStyle = computed(() => {
  const view = currentViewportRect.value
  return {
    left: `${percentX(view.x)}%`,
    top: `${percentY(view.y)}%`,
    width: `${Math.max(1.2, (view.w / overviewBounds.value.width) * 100)}%`,
    height: `${Math.max(1.2, (view.h / overviewBounds.value.height) * 100)}%`
  }
})

function severityColor(s) {
  return ['#98a2b3','#a66a1f','#a66a1f','#c2414b','#c2414b'][Math.min(s,5)-1] || '#98a2b3'
}

function formatTime(ms) {
  if (!ms) return ''
  const d = new Date(ms)
  return `${String(d.getHours()).padStart(2,'0')}:${String(d.getMinutes()).padStart(2,'0')}:${String(d.getSeconds()).padStart(2,'0')}`
}

function getQuality(id) {
  const dp = store.realtimeData[id]
  if (!dp) return 'bad'
  return (dp.quality || 'bad').toLowerCase()
}

function getDisplayValue(id) {
  const dp = store.realtimeData[id]
  return dp ? formatValue(dp.value, 1) : '---'
}

function formatValue(value, decimals = 1) {
  if (value === null || value === undefined || value === '') return '-'
  const num = Number(value)
  return Number.isFinite(num) ? num.toFixed(decimals) : String(value)
}

function clamp(value, min, max) {
  return Math.max(min, Math.min(max, value))
}

function navigateOverviewFromEvent(event) {
  const map = overviewMapRef.value
  if (!map) return
  const rect = map.getBoundingClientRect()
  if (rect.width <= 0 || rect.height <= 0) return
  const rx = clamp((event.clientX - rect.left) / rect.width, 0, 1)
  const ry = clamp((event.clientY - rect.top) / rect.height, 0, 1)
  const bounds = overviewBounds.value
  emit('minimap-navigate', {
    x: bounds.minX + rx * bounds.width,
    y: bounds.minY + ry * bounds.height
  })
}

function startOverviewDrag(event) {
  if (event.button !== 0) return
  event.preventDefault()
  overviewDragging.value = true
  navigateOverviewFromEvent(event)
  document.addEventListener('mousemove', handleOverviewDrag)
  document.addEventListener('mouseup', stopOverviewDrag)
}

function handleOverviewDrag(event) {
  if (!overviewDragging.value) return
  navigateOverviewFromEvent(event)
}

function stopOverviewDrag() {
  overviewDragging.value = false
  document.removeEventListener('mousemove', handleOverviewDrag)
  document.removeEventListener('mouseup', stopOverviewDrag)
}

function startResize(e) {
  emit('resize-start', e)
}

onMounted(() => {
  store.loadAlarms()
  store.loadLogs()
})

onUnmounted(() => {
  stopOverviewDrag()
})
</script>

<style scoped>
.bottom-dock {
  position: relative;
  background: linear-gradient(180deg, #ffffff 0%, #f9fafb 100%);
  border-top: 1px solid var(--border-light);
  display: flex;
  flex-direction: column;
  flex-shrink: 0;
  box-shadow: 0 -8px 18px rgba(16,24,40,0.04);
  min-height: 96px;
  overflow: hidden;
}

.bottom-dock-resizer {
  height: 9px;
  flex-shrink: 0;
  cursor: row-resize;
  display: flex;
  align-items: center;
  justify-content: center;
  background: #f1f5f9;
  border-bottom: 1px solid #e2e8f0;
  transition: background 0.16s ease-out;
}
.bottom-dock-resizer:hover {
  background: #e6f3ee;
}
.bottom-dock-resizer span {
  width: 42px;
  height: 3px;
  border-radius: 999px;
  background: #c6d1de;
  transition: background 0.16s ease-out, width 0.16s ease-out;
}
.bottom-dock-resizer:hover span {
  width: 54px;
  background: #1f8f5a;
}

.dock-tabs {
  display: flex;
  align-items: center;
  gap: 6px;
  min-height: 36px;
  padding: 4px 10px;
  background: rgba(248, 250, 252, 0.95);
  border-bottom: 1px solid var(--border-light);
  flex-shrink: 0;
}
.dock-tab {
  display: flex;
  align-items: center;
  gap: 6px;
  min-height: 28px;
  padding: 0 11px;
  font-size: var(--fs-xs);
  font-weight: var(--fw-semibold);
  color: var(--text-tertiary);
  border: 1px solid transparent;
  border-radius: 7px;
  transition: background var(--transition-fast), color var(--transition-fast), border-color var(--transition-fast), box-shadow var(--transition-fast);
}
.dock-tab:hover { color: var(--text-primary); background: #ffffff; box-shadow: 0 1px 2px rgba(15, 23, 42, 0.06); }
.dock-tab.active {
  color: #176e45;
  background: #eef7f2;
  border-color: rgba(31, 143, 90, 0.22);
}
.dock-badge {
  background: var(--info-bg);
  color: var(--info-text);
  border: 1px solid var(--info-border);
  padding: 1px 6px;
  border-radius: var(--radius-full);
  font-size: 12px;
  min-width: 18px;
  text-align: center;
}

.dock-content {
  flex: 1;
  overflow: hidden;
}
.dock-scroll {
  height: 100%;
  overflow-y: auto;
  padding: 3px 0 6px;
}
.dock-overview {
  height: 100%;
  display: grid;
  grid-template-columns: minmax(220px, 1fr) 160px;
  gap: 12px;
  padding: 10px 14px;
  background: #f8fafc;
}
.overview-map {
  position: relative;
  min-height: 74px;
  border: 1px solid #cfd9e3;
  border-radius: 8px;
  overflow: hidden;
  cursor: crosshair;
  background:
    linear-gradient(rgba(100, 116, 139, 0.13) 1px, transparent 1px),
    linear-gradient(90deg, rgba(100, 116, 139, 0.13) 1px, transparent 1px),
    #ffffff;
  background-size: 18px 18px;
  box-shadow: inset 0 0 0 1px rgba(255,255,255,0.55);
}
.overview-map.dragging {
  cursor: grabbing;
  border-color: rgba(31, 143, 90, 0.55);
}
.overview-empty {
  position: absolute;
  inset: 0;
  display: flex;
  align-items: center;
  justify-content: center;
  color: var(--text-placeholder);
  font-size: var(--fs-xs);
  font-weight: var(--fw-semibold);
}
.overview-widget {
  position: absolute;
  min-width: 4px;
  min-height: 4px;
  border-radius: 3px;
  background: rgba(79, 111, 184, 0.74);
  border: 1px solid rgba(46, 74, 140, 0.75);
  box-shadow: 0 1px 3px rgba(15, 23, 42, 0.14);
}
.overview-viewport {
  position: absolute;
  min-width: 12px;
  min-height: 10px;
  border: 2px solid #1f8f5a;
  border-radius: 6px;
  background: rgba(31, 143, 90, 0.11);
  box-shadow: 0 0 0 999px rgba(15, 23, 42, 0.04), 0 0 0 1px rgba(255,255,255,0.8) inset;
  pointer-events: none;
}
.overview-side {
  display: flex;
  flex-direction: column;
  justify-content: center;
  gap: 6px;
  min-width: 0;
  padding: 0 4px;
}
.overview-title {
  color: var(--text-primary);
  font-size: var(--fs-sm);
  font-weight: var(--fw-bold);
}
.overview-meta {
  color: var(--text-tertiary);
  font-size: var(--fs-xs);
}
.dock-empty {
  display: flex;
  align-items: center;
  justify-content: center;
  height: 100%;
  color: var(--text-placeholder);
  font-size: var(--fs-sm);
}
.dock-row {
  display: flex;
  align-items: center;
  gap: 10px;
  min-height: 29px;
  padding: 5px 14px;
  font-size: var(--fs-xs);
  border-bottom: 1px solid var(--border-light);
  animation: fadeIn 0.15s ease;
}
.dock-row:hover { background: #f8fafc; }
.dock-row.unack { background: var(--danger-bg); }

.dr-dot { width: 6px; height: 6px; border-radius: 50%; flex-shrink: 0; }
.dr-time { color: var(--text-placeholder); font-family: var(--font-mono); min-width: 60px; }
.dr-var { color: var(--text-secondary); font-weight: var(--fw-medium); min-width: 50px; }
.dr-type { flex-shrink: 0; }
.dr-msg { flex: 1; color: var(--text-primary); overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
.dr-val { color: var(--text-primary); font-family: var(--font-mono); min-width: 50px; text-align: right; }
.dr-unit { color: var(--text-placeholder); font-size: 12px; }
.dr-source { color: var(--text-placeholder); font-size: 12px; }

.dr-type-badge {
  padding: 3px 7px;
  border-radius: var(--radius-full);
  font-size: 12px;
  font-weight: var(--fw-semibold);
  border: 1px solid transparent;
}
.dr-type-badge.SET_VALUE { background: var(--info-bg); color: var(--info-text); border-color: var(--info-border); }
.dr-type-badge.WRITE_DB { background: var(--success-bg); color: var(--success-text); border-color: var(--success-border); }
.dr-type-badge.ALARM_ACK { background: var(--danger-bg); color: var(--danger-text); border-color: var(--danger-border); }


/* Bottom dock readability overrides. */
.bottom-dock {
  font-size: 13px;
}
.dock-tab,
.dock-row,
.overview-meta,
.dr-unit,
.dr-source,
.dr-type-badge {
  font-size: 13px;
}
.dock-badge {
  font-size: 12px;
}
.overview-title,
.dock-empty {
  font-size: 14px;
}
@media (max-width: 760px) {
  .dock-overview {
    grid-template-columns: 1fr;
  }
  .overview-side {
    display: none;
  }
}
</style>
