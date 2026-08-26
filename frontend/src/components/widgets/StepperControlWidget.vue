<template>
  <div class="stepper-control" :class="[stateClass, { disabled: !canAdjust }]" :title="config.title || '加减调节'">
    <button type="button" class="stepper-btn" :disabled="!canAdjust" @click.stop.prevent="changeValue(-stepValue)">{{ minusText }}</button>
    <div class="stepper-value"><span class="stepper-number">{{ displayValue }}</span><span v-if="unitText" class="stepper-unit">{{ unitText }}</span></div>
    <button type="button" class="stepper-btn" :disabled="!canAdjust" @click.stop.prevent="changeValue(stepValue)">{{ plusText }}</button>
  </div>
</template>

<script setup>
import { computed, ref, watch } from 'vue'
import { useMonitorStore } from '../../store/index.js'
import api from '../../api/index.js'

const props = defineProps({ widget: { type: Object, required: true } })
const store = useMonitorStore()
const stateClass = ref('')
const config = computed(() => props.widget?.config || {})
const varId = computed(() => config.value.varId)
const number = (value, fallback = 0) => Number.isFinite(Number(value)) ? Number(value) : fallback
const maybeNumber = value => value == null || value === '' || !Number.isFinite(Number(value)) ? null : Number(value)
const decimals = computed(() => Math.max(0, Math.min(6, Math.trunc(number(config.value.decimals, 0)))) )
const stepValue = computed(() => Math.max(Math.abs(number(config.value.step, 1)), Number.EPSILON))
const min = computed(() => maybeNumber(config.value.min))
const max = computed(() => maybeNumber(config.value.max))
const unitText = computed(() => String(config.value.unit || '').trim())
const minusText = computed(() => String(config.value.minusText || '-'))
const plusText = computed(() => String(config.value.plusText || '+'))
const localValue = ref(number(config.value.defaultValue, 0))
const optimisticValue = ref(null)
const realtimeValue = computed(() => varId.value == null ? null : maybeNumber(store.realtimeData[varId.value]?.value))
const currentValue = computed(() => optimisticValue.value ?? realtimeValue.value ?? localValue.value)
const displayValue = computed(() => decimals.value ? number(currentValue.value).toFixed(decimals.value) : String(Math.round(number(currentValue.value))))
const canAdjust = computed(() => stateClass.value !== 'loading')

watch(realtimeValue, value => { if (value != null) { localValue.value = value; optimisticValue.value = null } }, { immediate: true })
watch(() => config.value.defaultValue, value => { if (varId.value == null) localValue.value = number(value, 0) })

function clamp(value) {
  let next = value
  if (min.value != null) next = Math.max(min.value, next)
  if (max.value != null) next = Math.min(max.value, next)
  const factor = 10 ** decimals.value
  return Math.round((next + Number.EPSILON) * factor) / factor
}
function flash(next) { stateClass.value = next; window.setTimeout(() => { if (stateClass.value === next) stateClass.value = '' }, 800) }
async function changeValue(delta) {
  if (!canAdjust.value) return
  const next = clamp(number(currentValue.value) + number(delta))
  if (varId.value == null || varId.value === '') { localValue.value = next; flash('success'); return }
  stateClass.value = 'loading'; optimisticValue.value = next; localValue.value = next
  try { await api.writeVariable(varId.value, next); flash('success') } catch { optimisticValue.value = null; flash('error') }
}
</script>

<style scoped>
.stepper-control { width:100%; height:100%; display:grid; grid-template-columns:minmax(24px,28%) 1fr minmax(24px,28%); align-items:stretch; border:1px solid #9ca3af; border-radius:2px; overflow:hidden; background:linear-gradient(180deg,#f8fafc,#e5e7eb); box-shadow:inset 1px 1px 0 rgba(255,255,255,.92),inset -1px -1px 0 rgba(75,85,99,.24),0 1px 2px rgba(15,23,42,.1); color:#111827; user-select:none; }
.stepper-btn { min-width:0; min-height:0; border:0; border-right:1px solid rgba(100,116,139,.45); background:linear-gradient(180deg,rgba(255,255,255,.92),rgba(226,232,240,.86)); color:#111827; font-family:"Microsoft YaHei",Arial,sans-serif; font-size:clamp(12px,32%,18px); font-weight:800; cursor:pointer; }.stepper-btn:last-child { border-right:0; border-left:1px solid rgba(100,116,139,.45); }.stepper-btn:hover:not(:disabled) { filter:brightness(1.05); }.stepper-btn:active:not(:disabled) { transform:translateY(1px); }
.stepper-value { min-width:0; display:flex; align-items:center; justify-content:center; gap:4px; padding:0 6px; background:linear-gradient(180deg,rgba(255,255,255,.88),rgba(226,232,240,.78)); font-family:"Microsoft YaHei",Arial,sans-serif; }.stepper-number { min-width:0; overflow:hidden; text-overflow:ellipsis; white-space:nowrap; font-size:clamp(11px,28%,16px); font-weight:800; color:#0f172a; }.stepper-unit { flex:0 0 auto; font-size:clamp(10px,22%,13px); font-weight:700; color:#475569; }
.loading,.disabled { opacity:.7; }.success { outline:2px solid rgba(34,197,94,.28); }.error { outline:2px solid rgba(239,68,68,.34); }
</style>
