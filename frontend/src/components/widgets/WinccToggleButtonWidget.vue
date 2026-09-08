<template>
  <button
    type="button"
    class="wincc-toggle"
    :class="[stateClass, { active: isPressed, disabled: !canWrite }]"
    :style="buttonStyle"
    :title="buttonTitle"
    @click.stop="handleClick"
  >
    <span class="toggle-text">{{ displayText }}</span>
  </button>
  <ConfirmDialog
    v-if="commandDialog"
    :title="commandDialog.title"
    :message="commandDialog.message"
    :detail="commandDialog.detail"
    :confirm-text="commandDialog.confirmText"
    @close="closeCommandDialog"
    @confirm="closeCommandDialog"
  />
</template>

<script setup>
import { computed, ref } from 'vue'
import { useMonitorStore } from '../../store/index.js'
import ConfirmDialog from '../common/ConfirmDialog.vue'
import { writeCollectedVariable } from '../../services/automaticParameterService.js'

// Shared by every status-toggle instance. When controls in one group are
// clicked rapidly, only the newest confirmed command updates the group color.
const latestExclusiveOperation = new Map()

const props = defineProps({
  widget: { type: Object, required: true }
})

const store = useMonitorStore()
const stateClass = ref('')
const commandDialog = ref(null)
const config = computed(() => props.widget.config || {})
const writeValue = computed(() => normalizeValue(
  Object.prototype.hasOwnProperty.call(config.value, 'writeValue')
    ? config.value.writeValue
    : config.value.activeValue,
  1
))
const isPressed = computed(() => Boolean(config.value.pressed))
const canWrite = computed(() => config.value.varId != null && stateClass.value !== 'loading')
const displayText = computed(() => config.value.buttonText || config.value.title || '状态写入按钮')
const buttonTitle = computed(() => {
  if (config.value.varId == null) return '请先在属性面板关联采集变量'
  return `按下将写入 ${writeValue.value}`
})
const buttonStyle = computed(() => ({
  '--wincc-active-color': config.value.activeColor || '#18c93a',
  '--wincc-inactive-top': config.value.inactiveTopColor || '#ffffff',
  '--wincc-inactive-bottom': config.value.inactiveBottomColor || '#e5e7eb'
}))
const exclusivePeers = computed(() => {
  const widgetId = String(props.widget.id)
  const buttons = store.widgets.filter(widget => widget?.type === 'winccToggleButton')
  const byId = new Map(buttons.map(widget => [String(widget.id), widget]))
  const visited = new Set([widgetId])
  const queue = [widgetId]
  while (queue.length) {
    const currentId = queue.shift()
    const current = byId.get(currentId)
    const currentPeers = peerIdsFor(current?.config)
    buttons.forEach((candidate) => {
      const candidateId = String(candidate.id)
      if (candidateId === currentId || visited.has(candidateId)) return
      if (currentPeers.has(candidateId) || peerIdsFor(candidate.config).has(currentId)) {
        visited.add(candidateId)
        queue.push(candidateId)
      }
    })
  }
  return buttons.filter(widget => String(widget.id) !== widgetId && visited.has(String(widget.id)))
})
const exclusiveOperationKey = computed(() => [String(props.widget.id), ...exclusivePeers.value.map(peer => String(peer.id))]
  .sort()
  .join('|'))

function normalizeValue(value, fallback) {
  if (value === null || value === undefined || value === '') return fallback
  const number = Number(value)
  return Number.isFinite(number) ? number : String(value)
}

function peerIdsFor(peerConfig) {
  if (!peerConfig?.exclusiveEnabled || !Array.isArray(peerConfig.exclusivePeerIds)) return new Set()
  return new Set(peerConfig.exclusivePeerIds.map(id => String(id)))
}

function markPressedAndResetPeers() {
  config.value.pressed = true
  exclusivePeers.value.forEach((peer) => {
    if (peer.config && typeof peer.config === 'object') peer.config.pressed = false
  })
}

function showCommandError(error) {
  commandDialog.value = {
    title: '状态写入未下发',
    message: error?.message || '控制器通信异常，未能写入设定值。',
    detail: '请检查通信资源是否已启动、采集变量是否可写，以及控制器是否返回应答或回读值。',
    confirmText: '知道了'
  }
}

function closeCommandDialog() {
  commandDialog.value = null
}

async function handleClick() {
  if (!canWrite.value) return
  const operationKey = exclusiveOperationKey.value
  const operationId = Symbol('exclusive-operation')
  latestExclusiveOperation.set(operationKey, operationId)
  stateClass.value = 'loading'
  try {
    await writeCollectedVariable(config.value.varId, writeValue.value)
    await store.loadRealtime().catch(() => {})
    if (latestExclusiveOperation.get(operationKey) === operationId) markPressedAndResetPeers()
  } catch (error) {
    showCommandError(error)
  } finally {
    if (latestExclusiveOperation.get(operationKey) === operationId) latestExclusiveOperation.delete(operationKey)
    stateClass.value = ''
  }
}
</script>

<style scoped>
.wincc-toggle {
  width: 100%;
  height: 100%;
  min-width: 0;
  min-height: 0;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  padding: 0 14px;
  border: 1px solid #9ca3af;
  border-radius: 2px;
  background:
    linear-gradient(180deg, rgba(255,255,255,0.92), rgba(255,255,255,0.18) 48%, rgba(0,0,0,0.08) 50%, rgba(255,255,255,0.24)),
    linear-gradient(180deg, var(--wincc-inactive-top), var(--wincc-inactive-bottom));
  color: #111827;
  font-family: var(--font-family);
  font-size: clamp(12px, 33%, 20px);
  font-weight: 800;
  line-height: 1.1;
  text-align: center;
  white-space: nowrap;
  cursor: pointer;
  user-select: none;
  box-shadow:
    inset 1px 1px 0 rgba(255,255,255,0.92),
    inset -1px -1px 0 rgba(75,85,99,0.36),
    0 1px 1px rgba(15,23,42,0.14);
  transition: filter 0.12s ease, transform 0.08s ease, box-shadow 0.12s ease;
}
.wincc-toggle:hover {
  filter: brightness(1.04);
}
.wincc-toggle:active,
.wincc-toggle.loading {
  transform: translateY(1px);
  box-shadow:
    inset -1px -1px 0 rgba(255,255,255,0.65),
    inset 1px 1px 0 rgba(15,23,42,0.22);
}
.wincc-toggle.active {
  border-color: color-mix(in srgb, var(--wincc-active-color), #263544 35%);
  background:
    linear-gradient(180deg, rgba(255,255,255,0.72), rgba(255,255,255,0.08) 48%, rgba(0,0,0,0.12) 50%, rgba(255,255,255,0.12)),
    linear-gradient(180deg, color-mix(in srgb, var(--wincc-active-color), #ffffff 42%), var(--wincc-active-color) 54%, color-mix(in srgb, var(--wincc-active-color), #000000 24%));
  color: #07130a;
  text-shadow: 0 1px 0 rgba(255,255,255,0.35);
}
.wincc-toggle.disabled {
  cursor: default;
  opacity: 0.72;
}
.wincc-toggle .toggle-text {
  overflow: hidden;
  text-overflow: ellipsis;
  max-width: 100%;
}
</style>
