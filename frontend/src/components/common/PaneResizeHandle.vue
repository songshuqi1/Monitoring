<template>
  <div class="pane-resize-handle" :class="[axis === 'y' ? 'horizontal' : 'vertical', { dragging }]"
    role="separator" tabindex="0" :aria-label="label" :title="label"
    :aria-orientation="axis === 'y' ? 'horizontal' : 'vertical'" :aria-valuenow="Math.round(modelValue)" :aria-valuemin="min" :aria-valuemax="maximum()"
    @pointerdown.prevent="start" @pointermove="move" @pointerup="finish" @pointercancel="finish" @lostpointercapture="finish" @keydown="keyboard">
    <span></span>
  </div>
</template>

<script setup>
import { ref, onBeforeUnmount } from 'vue'
const props = defineProps({ modelValue: Number, min: { type: Number, default: 120 }, max: { type: [Number, Function], default: 2000 }, axis: { type: String, default: 'x' }, reverse: Boolean, label: String })
const emit = defineEmits(['update:modelValue', 'resize-start', 'resize-end'])
const dragging = ref(false)
let startCoordinate = 0, startSize = 0, cursor = '', userSelect = '', target = null, pointerId = null
const maximum = () => Math.max(props.min, Number(typeof props.max === 'function' ? props.max() : props.max) || props.min)
const update = value => emit('update:modelValue', Math.round(Math.max(props.min, Math.min(maximum(), value))))
function start(event) {
  if (event.button !== 0) return
  startCoordinate = props.axis === 'y' ? event.clientY : event.clientX
  startSize = props.modelValue
  target = event.currentTarget; pointerId = event.pointerId
  cursor = document.body.style.cursor; userSelect = document.body.style.userSelect
  document.body.style.cursor = props.axis === 'y' ? 'row-resize' : 'col-resize'
  document.body.style.userSelect = 'none'
  dragging.value = true
  target.setPointerCapture(pointerId)
  emit('resize-start')
}
function move(event) {
  if (!dragging.value) return
  const coordinate = props.axis === 'y' ? event.clientY : event.clientX
  update(startSize + (coordinate - startCoordinate) * (props.reverse ? -1 : 1))
}
function finish() {
  if (!dragging.value) return
  dragging.value = false
  document.body.style.cursor = cursor; document.body.style.userSelect = userSelect
  if (target?.hasPointerCapture(pointerId)) target.releasePointerCapture(pointerId)
  target = null
  emit('resize-end')
}
function keyboard(event) {
  const direction = props.axis === 'y' ? { ArrowUp: -1, ArrowDown: 1 } : { ArrowLeft: -1, ArrowRight: 1 }
  if (direction[event.key]) { event.preventDefault(); update(props.modelValue + direction[event.key] * (props.reverse ? -1 : 1) * (event.shiftKey ? 50 : 10)) }
  else if (event.key === 'Home' || event.key === 'End') { event.preventDefault(); update(event.key === 'Home' ? props.min : maximum()) }
}
onBeforeUnmount(finish)
</script>

<style scoped>
.pane-resize-handle { flex-shrink: 0; position: relative; background: #eef3f7; touch-action: none; user-select: none; z-index: 5; }
.vertical { width: 7px; cursor: col-resize; }.horizontal { height: 7px; cursor: row-resize; }
.pane-resize-handle span { position: absolute; background: #b5c6d3; border-radius: 3px; }
.vertical span { width: 2px; height: 40px; top: calc(50% - 20px); left: 2px; }.horizontal span { height: 2px; width: 40px; left: calc(50% - 20px); top: 2px; }
.pane-resize-handle:hover,.pane-resize-handle:focus-visible,.dragging { background: #d8e7f1; outline: none; }.pane-resize-handle:hover span,.dragging span { background: #5184a6; }
</style>
