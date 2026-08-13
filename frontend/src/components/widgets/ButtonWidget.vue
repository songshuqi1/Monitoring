<template>
  <div class="bw" :class="stateClass" @click="handleClick">
    <span>{{ widget.config.buttonText || widget.config.title || '按钮' }}</span>
  </div>
</template>

<script setup>
import { ref } from 'vue'
import api from '../../api/index.js'
const props = defineProps({ widget: { type: Object, required: true } })
const stateClass = ref('')
function handleClick() {
  const { varId, writeValue } = props.widget.config
  if (varId == null || writeValue == null) return
  stateClass.value = 'loading'
  api.writeVariable(varId, writeValue)
    .then(() => { stateClass.value = 'success'; setTimeout(() => stateClass.value = '', 800) })
    .catch(() => { stateClass.value = 'error'; setTimeout(() => stateClass.value = '', 800) })
}
</script>

<style scoped>
.bw {
  width: 100%; height: 100%;
  display: flex; align-items: center; justify-content: center;
  cursor: pointer; user-select: none;
  font-size: var(--fs-sm); font-weight: var(--fw-semibold);
  color: var(--accent); background: var(--accent-soft);
  border: 1px solid var(--accent);
  border-radius: var(--radius-sm);
  transition: background var(--transition-fast), color var(--transition-fast), transform var(--transition-fast), box-shadow var(--transition-fast), border-color var(--transition-fast);
}
.bw:hover { background: var(--accent); color: var(--text-inverse); box-shadow: 0 8px 16px rgba(79,111,184,0.18); }
.bw:active { transform: scale(0.97); }
.bw.loading { opacity: 0.6; }
.bw.success { background: var(--success-bg); border-color: var(--success-border); color: var(--success-text); }
.bw.error { background: var(--danger-bg); border-color: var(--danger-border); color: var(--danger-text); }
</style>
