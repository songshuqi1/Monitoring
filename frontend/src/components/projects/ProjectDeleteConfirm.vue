<template>
  <div class="dialog-overlay" @click.self="$emit('close')">
    <div class="dialog delete-dialog">
      <h3>删除项目</h3>
      <p class="delete-copy">
        项目「{{ project.name }}」会被标记为删除，并可在当前会话中撤销。
      </p>
      <div class="delete-meta">
        <span>{{ layoutWidgetCount(project.layout) }} 个组件</span>
        <span>{{ layoutConnectionCount(project.layout) }} 条线</span>
        <span>更新 {{ formatDate(project.updatedAt) }}</span>
      </div>
      <div class="dialog-actions">
        <button class="btn btn-secondary" @click="$emit('close')">取消</button>
        <button class="btn btn-danger" :disabled="submitting" @click="$emit('confirm')">
          {{ submitting ? '删除中...' : '确认删除' }}
        </button>
      </div>
    </div>
  </div>
</template>

<script setup>
defineProps({
  project: { type: Object, required: true },
  submitting: { type: Boolean, default: false }
})

defineEmits(['close', 'confirm'])

function formatDate(ts) {
  return ts ? new Date(ts).toLocaleString('zh-CN', { hour12: false }) : '-'
}

function layoutWidgetCount(layout) {
  if (Array.isArray(layout)) return layout.length
  return Array.isArray(layout?.widgets) ? layout.widgets.length : 0
}

function layoutConnectionCount(layout) {
  return Array.isArray(layout?.connections) ? layout.connections.length : 0
}
</script>

<style scoped>
.delete-dialog {
  width: 420px;
}
.delete-copy {
  color: var(--text-secondary);
  font-size: var(--fs-sm);
}
.delete-meta {
  display: flex;
  gap: 12px;
  margin-top: 14px;
  padding: 10px 12px;
  border-radius: var(--radius-md);
  border: 1px solid var(--danger-border);
  background: var(--danger-bg);
  color: var(--danger-text);
  font-size: var(--fs-xs);
  font-family: var(--font-mono);
}
</style>
