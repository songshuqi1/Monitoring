<template>
  <div class="dialog-overlay" @mousedown.stop @click.self="$emit('close')">
    <div class="dialog confirm-dialog">
      <h3>{{ title }}</h3>
      <p class="confirm-message">{{ message }}</p>
      <p v-if="detail" class="confirm-detail">{{ detail }}</p>
      <div class="dialog-actions">
        <button class="btn btn-secondary" @click="$emit('close')">{{ cancelText }}</button>
        <button
          class="btn"
          :class="danger ? 'btn-danger' : 'btn-primary'"
          :disabled="submitting"
          @click="$emit('confirm')"
        >
          {{ submitting ? submittingText : confirmText }}
        </button>
      </div>
    </div>
  </div>
</template>

<script setup>
defineProps({
  title: { type: String, default: '确认操作' },
  message: { type: String, required: true },
  detail: { type: String, default: '' },
  confirmText: { type: String, default: '确认' },
  cancelText: { type: String, default: '取消' },
  submittingText: { type: String, default: '处理中...' },
  submitting: { type: Boolean, default: false },
  danger: { type: Boolean, default: false }
})

defineEmits(['close', 'confirm'])
</script>

<style scoped>
.confirm-dialog {
  width: min(420px, 92vw);
}
.confirm-message {
  color: var(--text-secondary);
  font-size: var(--fs-sm);
  line-height: 1.7;
}
.confirm-detail {
  margin-top: 10px;
  padding: 10px 12px;
  border: 1px solid var(--border-light);
  border-radius: var(--radius-md);
  background: var(--surface-muted);
  color: var(--text-tertiary);
  font-size: var(--fs-xs);
  line-height: 1.6;
}
</style>
