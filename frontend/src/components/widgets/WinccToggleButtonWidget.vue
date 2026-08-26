<template>
  <button type="button" class="wincc-toggle" :class="[stateClass, { active: isActive, disabled: !canWrite }]" :style="buttonStyle" :title="buttonTitle" @click.stop="handleClick">
    <span>{{ displayText }}</span>
  </button>
</template>

<script setup>
import { computed, ref } from 'vue'
import { useMonitorStore } from '../../store/index.js'
import api from '../../api/index.js'

const props = defineProps({ widget: { type: Object, required: true } })
const store = useMonitorStore()
const stateClass = ref('')
const config = computed(() => props.widget?.config || {})
const dataPoint = computed(() => config.value.varId != null ? store.realtimeData[config.value.varId] : null)
const normalizeValue = (value, fallback) => {
  if (value === null || value === undefined || value === '') return fallback
  const number = Number(value)
  return Number.isFinite(number) ? number : String(value)
}
const activeValue = computed(() => normalizeValue(config.value.activeValue, 1))
const inactiveValue = computed(() => normalizeValue(config.value.inactiveValue, 0))
const currentValue = computed(() => normalizeValue(dataPoint.value?.value, null))
const valuesEqual = (left, right) => typeof left === 'number' && typeof right === 'number' ? Math.abs(left - right) < 0.000001 : left != null && String(left) === String(right)
const isActive = computed(() => valuesEqual(currentValue.value, activeValue.value))
const canWrite = computed(() => config.value.varId != null && stateClass.value !== 'loading')
const displayText = computed(() => config.value.buttonText || config.value.title || '状态切换')
const buttonTitle = computed(() => config.value.varId == null ? '未绑定变量' : (isActive.value ? `写入 ${inactiveValue.value}` : `写入 ${activeValue.value}`))
const buttonStyle = computed(() => ({ '--wincc-active-color': config.value.activeColor || '#18c93a', '--wincc-inactive-top': config.value.inactiveTopColor || '#ffffff', '--wincc-inactive-bottom': config.value.inactiveBottomColor || '#e5e7eb' }))
const exclusivePeers = computed(() => {
  if (!config.value.exclusiveEnabled || !Array.isArray(config.value.exclusivePeerIds)) return []
  const ids = new Set(config.value.exclusivePeerIds.map(String))
  return store.widgets.filter(widget => widget?.type === 'winccToggleButton' && String(widget.id) !== String(props.widget.id) && ids.has(String(widget.id)) && widget.config?.varId != null)
})

async function handleClick() {
  if (!canWrite.value) return
  stateClass.value = 'loading'
  const next = isActive.value ? inactiveValue.value : activeValue.value
  try {
    await api.writeVariable(config.value.varId, next)
    if (config.value.exclusiveEnabled && valuesEqual(next, activeValue.value)) {
      const results = await Promise.allSettled(exclusivePeers.value.map(peer => api.writeVariable(peer.config.varId, normalizeValue(peer.config.inactiveValue, 0))))
      if (results.some(result => result.status === 'rejected')) throw new Error('exclusive peer write failed')
    }
    stateClass.value = 'success'
  } catch {
    stateClass.value = 'error'
  } finally {
    window.setTimeout(() => { stateClass.value = '' }, 800)
  }
}
</script>

<style scoped>
.wincc-toggle { width: 100%; height: 100%; min-width: 0; min-height: 0; display: flex; align-items: center; justify-content: center; padding: 0 14px; border: 1px solid #9ca3af; border-radius: 2px; background: linear-gradient(180deg,rgba(255,255,255,.92),rgba(255,255,255,.18) 48%,rgba(0,0,0,.08) 50%,rgba(255,255,255,.24)),linear-gradient(180deg,var(--wincc-inactive-top),var(--wincc-inactive-bottom)); color: #111827; font-family: "Microsoft YaHei",Arial,sans-serif; font-size: clamp(12px,33%,20px); font-weight: 800; line-height: 1.1; text-align: center; white-space: nowrap; cursor: pointer; user-select: none; box-shadow: inset 1px 1px 0 rgba(255,255,255,.92), inset -1px -1px 0 rgba(75,85,99,.36), 0 1px 1px rgba(15,23,42,.14); transition: filter .12s ease,transform .08s ease,box-shadow .12s ease; }
.wincc-toggle:hover { filter: brightness(1.04); }.wincc-toggle:active,.wincc-toggle.loading { transform: translateY(1px); box-shadow: inset -1px -1px 0 rgba(255,255,255,.65),inset 1px 1px 0 rgba(15,23,42,.22); }
.wincc-toggle.active { border-color: #0b9f2b; background: linear-gradient(180deg,rgba(255,255,255,.7),rgba(255,255,255,.06) 48%,rgba(0,0,0,.12) 50%,rgba(255,255,255,.12)),linear-gradient(180deg,#7cff7c 0%,var(--wincc-active-color) 55%,#04a71f 100%); color: #061706; text-shadow: 0 1px 0 rgba(255,255,255,.35); }
.wincc-toggle.disabled { cursor: default; opacity: .72; }.wincc-toggle.success { outline: 2px solid rgba(34,197,94,.32); }.wincc-toggle.error { outline: 2px solid rgba(239,68,68,.42); }.wincc-toggle span { overflow: hidden; text-overflow: ellipsis; }
</style>
