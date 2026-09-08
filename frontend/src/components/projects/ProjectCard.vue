<template>
  <article class="project-card" :class="{ deleted: project.deletedAt }">
    <div class="project-card-main">
      <div class="project-title-row">
        <h4>{{ project.name }}</h4>
        <span v-if="project.deletedAt" class="badge badge-err">已删除</span>
      </div>
      <p class="project-desc">{{ project.description || '暂无描述' }}</p>
      <div class="project-meta">
        <span>{{ layoutWidgetCount(project.layout) }} 个组件</span>
        <span>{{ layoutConnectionCount(project.layout) }} 条线</span>
        <span>{{ project.layout?.scene3d?.nodes?.length || 0 }} 个 3D 组件</span>
        <span>更新 {{ formatDate(project.updatedAt) }}</span>
      </div>
    </div>

    <div class="project-actions">
      <button class="btn btn-sm btn-primary" :disabled="project.deletedAt" @click="$emit('load', project)">加载</button>
      <button class="btn btn-sm" @click="$emit('detail', project)">详情</button>
      <button class="btn btn-sm" :disabled="project.deletedAt" @click="$emit('edit', project)">编辑</button>
      <button class="btn btn-sm" :disabled="project.deletedAt" @click="$emit('duplicate', project)">复制</button>
      <button class="btn btn-sm" :disabled="project.deletedAt" @click="$emit('export', project)">导出</button>
      <button class="btn btn-sm btn-danger" :disabled="!canDelete || project.deletedAt" @click="$emit('delete', project)">
        删除
      </button>
    </div>
  </article>
</template>

<script setup>
defineProps({
  project: { type: Object, required: true },
  canDelete: { type: Boolean, default: false }
})

defineEmits(['load', 'detail', 'edit', 'duplicate', 'export', 'delete'])

function formatDate(ts) {
  if (!ts) return '-'
  return new Date(ts).toLocaleString('zh-CN', { hour12: false })
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
.project-card {
  display: grid;
  grid-template-columns: minmax(340px, 1fr) auto;
  align-items: center;
  gap: 20px;
  min-height: 112px;
  padding: 18px 20px;
  border: 1px solid var(--border-light);
  border-radius: var(--radius-lg);
  background: var(--bg-primary);
  box-shadow: var(--shadow-card);
  transition: border-color var(--transition-fast), box-shadow var(--transition-fast), background var(--transition-fast);
}
.project-card:hover {
  border-color: var(--border-color);
  box-shadow: var(--shadow-sm);
}
.project-card.deleted {
  background: var(--surface-muted);
  opacity: 0.72;
}
.project-card-main {
  min-width: 0;
  flex: 1;
}
.project-title-row {
  display: flex;
  align-items: center;
  gap: 8px;
  margin-bottom: 4px;
}
.project-title-row h4 {
  min-width: 0;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
  font-size: var(--fs-lg);
  font-weight: var(--fw-bold);
  color: var(--text-primary);
}
.project-desc {
  min-height: 18px;
  color: var(--text-muted);
  font-size: var(--fs-sm);
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.project-meta {
  display: flex;
  align-items: center;
  gap: 16px;
  margin-top: 8px;
  color: var(--text-placeholder);
  font-size: var(--fs-xs);
  font-family: var(--font-mono);
}
.project-meta span {
  white-space: nowrap;
}
.project-actions {
  display: flex;
  align-items: center;
  gap: 6px;
  flex-wrap: nowrap;
  justify-content: flex-end;
}

@media (max-width: 920px) {
  .project-card {
    grid-template-columns: minmax(0, 1fr);
    align-items: stretch;
    gap: 14px;
  }
  .project-actions {
    justify-content: flex-start;
  }
}
@media (max-width: 560px) {
  .project-card { padding: 16px; }
  .project-meta { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 6px 12px; }
  .project-actions { flex-wrap: wrap; }
}
</style>
