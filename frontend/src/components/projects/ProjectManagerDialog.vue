<template>
  <div class="dialog-overlay" @click.self="$emit('close')">
    <div class="dialog dialog-project-manager">
      <div class="pm-header">
        <div>
          <h3>项目管理</h3>
          <p>保存、筛选、加载和维护画布项目</p>
        </div>
        <button class="btn-icon" @click="$emit('close')">✕</button>
      </div>

      <div class="pm-toolbar">
        <div class="pm-filter-group">
          <input v-model="searchText" placeholder="搜索项目名称或描述" />
          <select v-model="sortValue" @change="applySort">
            <option value="updatedAt:desc">最近更新</option>
            <option value="createdAt:desc">最近创建</option>
            <option value="name:asc">名称 A-Z</option>
            <option value="name:desc">名称 Z-A</option>
          </select>
          <select v-model.number="projectStore.query.pageSize">
            <option :value="6">每页 6 个</option>
            <option :value="8">每页 8 个</option>
            <option :value="12">每页 12 个</option>
          </select>
        </div>
        <div class="pm-action-group">
          <button class="btn btn-primary" @click="saveCurrentLayout">
            {{ projectStore.currentProject ? '更新当前项目' : '保存当前画布' }}
          </button>
          <button v-if="projectStore.currentProject" class="btn" @click="openCreate">另存为新项目</button>
          <button class="btn" @click="exportCurrentLayout">导出当前画布</button>
          <button class="btn" @click="triggerImport">导入项目</button>
        </div>
        <input ref="importInput" class="pm-file-input" type="file" accept=".json,application/json" @change="importProjectFile" />
      </div>

      <div v-if="projectStore.errorMessage" class="pm-error">{{ projectStore.errorMessage }}</div>
      <div v-if="fileMessage" class="pm-note">{{ fileMessage }}</div>

      <div class="pm-list">
        <ProjectCard
          v-for="project in projectStore.list"
          :key="project.id"
          :project="project"
          :can-delete="projectStore.canDelete"
          @load="loadProject"
          @detail="openDetail"
          @edit="openEdit"
          @duplicate="duplicateProject"
          @export="exportProject"
          @delete="openDelete"
        />
        <div v-if="!projectStore.loading && projectStore.list.length === 0" class="pm-empty">
          暂无项目，保存当前画布后会显示在这里
        </div>
        <div v-if="projectStore.loading" class="pm-empty">加载中...</div>
      </div>

      <div class="pm-footer">
        <span>共 {{ projectStore.total }} 个项目</span>
        <div class="pm-pages">
          <button class="btn btn-sm" :disabled="projectStore.query.page <= 1" @click="changePage(projectStore.query.page - 1)">上一页</button>
          <span>{{ projectStore.query.page }} / {{ projectStore.totalPages }}</span>
          <button class="btn btn-sm" :disabled="projectStore.query.page >= projectStore.totalPages" @click="changePage(projectStore.query.page + 1)">下一页</button>
        </div>
      </div>

      <div v-if="projectStore.undoDeleteProject" class="pm-toast">
        <span>已删除「{{ projectStore.undoDeleteProject.name }}」</span>
        <button class="btn btn-xs btn-primary" @click="restoreDeleted">撤销</button>
        <button class="btn-icon" @click="projectStore.clearUndo">✕</button>
      </div>

      <div v-if="copyMessage" class="pm-toast pm-toast-copy">
        <span>{{ copyMessage }}</span>
        <button class="btn-icon" @click="copyMessage = ''">✕</button>
      </div>
    </div>

    <ProjectFormModal
      v-if="formOpen"
      :mode="formMode"
      :project="editingProject"
      :current-layout="currentLayout"
      :submitting="projectStore.loading"
      @close="formOpen = false"
      @submit="submitForm"
    />

    <ProjectDeleteConfirm
      v-if="deleteTarget"
      :project="deleteTarget"
      :submitting="projectStore.loading"
      @close="deleteTarget = null"
      @confirm="confirmDelete"
    />

    <div v-if="detailProject" class="dialog-overlay" @click.self="detailProject = null">
      <div class="dialog project-detail-dialog">
        <h3>项目详情</h3>
        <div class="detail-grid">
          <span>名称</span><strong>{{ detailProject.name }}</strong>
          <span>描述</span><strong>{{ detailProject.description || '-' }}</strong>
          <span>组件数量</span><strong>{{ layoutWidgetCount(detailProject.layout) }}</strong>
          <span>连接线</span><strong>{{ layoutConnectionCount(detailProject.layout) }}</strong>
          <span>3D 组件</span><strong>{{ detailProject.layout?.scene3d?.nodes?.length || 0 }}</strong>
          <span>创建时间</span><strong>{{ formatDate(detailProject.createdAt) }}</strong>
          <span>更新时间</span><strong>{{ formatDate(detailProject.updatedAt) }}</strong>
        </div>
        <div class="dialog-actions">
          <button class="btn btn-secondary" @click="detailProject = null">关闭</button>
          <button class="btn btn-primary" :disabled="detailProject.deletedAt" @click="loadProject(detailProject)">加载</button>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { onMounted, ref, watch } from 'vue'
import { useMonitorStore } from '../../store/index.js'
import { useProjectStore } from '../../store/projectStore.js'
import { normalizeScadaProjectImport } from '../../services/scadaProjectImport.js'
import { projectVariableReferences, validateImportedProjectBindings } from '../../services/scene3dService.js'
import ProjectCard from './ProjectCard.vue'
import ProjectDeleteConfirm from './ProjectDeleteConfirm.vue'
import ProjectFormModal from './ProjectFormModal.vue'

const props = defineProps({
  currentLayout: { type: [Array, Object], default: () => ({ widgets: [], connections: [] }) },
  openCreateOnMount: { type: Boolean, default: false }
})

const emit = defineEmits(['close', 'load-layout'])
const monitorStore = useMonitorStore()
const projectStore = useProjectStore()
const searchText = ref(projectStore.query.search)
const sortValue = ref(`${projectStore.query.sortBy}:${projectStore.query.sortDir}`)
const formOpen = ref(false)
const formMode = ref('create')
const editingProject = ref(null)
const deleteTarget = ref(null)
const detailProject = ref(null)
const importInput = ref(null)
const fileMessage = ref('')
let searchTimer = null

onMounted(async () => {
  projectStore.loadQueryFromUrl()
  searchText.value = projectStore.query.search
  sortValue.value = `${projectStore.query.sortBy}:${projectStore.query.sortDir}`
  await projectStore.fetchProjects().catch(() => {})
  if (props.openCreateOnMount && !projectStore.currentProject) openCreate()
})

watch(searchText, (value) => {
  clearTimeout(searchTimer)
  searchTimer = setTimeout(() => {
    projectStore.setQuery({ search: value.trim() })
  }, 250)
})

function applySort() {
  const [sortBy, sortDir] = sortValue.value.split(':')
  projectStore.setQuery({ sortBy, sortDir })
}

function openCreate() {
  formMode.value = 'create'
  editingProject.value = null
  formOpen.value = true
}

async function saveCurrentLayout() {
  const current = projectStore.currentProject
  if (!current) {
    openCreate()
    return
  }
  try {
    const updated = await projectStore.updateProject(current.id, {
      name: current.name,
      description: current.description || '',
      layout: props.currentLayout
    })
    monitorStore.markProjectSaved(updated.id)
    fileMessage.value = `已更新「${updated.name}」`
  } catch {
    // projectStore.errorMessage provides the user-facing message.
  }
}

function openEdit(project) {
  formMode.value = 'edit'
  editingProject.value = project
  formOpen.value = true
}

function openDelete(project) {
  deleteTarget.value = project
}

function openDetail(project) {
  detailProject.value = project
}

async function submitForm(payload) {
  try {
    if (formMode.value === 'edit' && editingProject.value) {
      await projectStore.updateProject(editingProject.value.id, payload)
    } else {
      const project = await projectStore.createProject(payload)
      projectStore.setCurrentProject(project)
      monitorStore.markProjectSaved(project.id)
    }
    formOpen.value = false
  } catch {
    // projectStore.errorMessage provides the user-facing message.
  }
}

async function confirmDelete() {
  if (!deleteTarget.value) return
  try {
    await projectStore.softDeleteProject(deleteTarget.value)
    deleteTarget.value = null
  } catch {
    // projectStore.errorMessage provides the user-facing message.
  }
}

async function restoreDeleted() {
  await projectStore.restoreLastDeleted().catch(() => {})
}

const copyMessage = ref('')

async function duplicateProject(project) {
  try {
    const copied = await projectStore.duplicateProject(project)
    copyMessage.value = `已复制为「${copied.name}」`
    window.setTimeout(() => { copyMessage.value = '' }, 3000)
  } catch {
    // projectStore.errorMessage provides the user-facing message.
  }
}

function safeFileName(name) {
  return String(name || 'monitoring-project')
    .replace(/[\\/:*?"<>|]/g, '_')
    .replace(/\s+/g, '_')
    .slice(0, 80)
}

function exportPayload(project) {
  return {
    type: 'monitoring-platform-project',
    version: 2,
    exportedAt: new Date().toISOString(),
    project: {
      name: project.name,
      description: project.description || '',
      layout: project.layout
    },
    customComponents: monitorStore.customComponents,
    variableReferences: projectVariableReferences(project.layout, monitorStore.variables)
  }
}

async function saveJsonFile(filename, payload) {
  const json = JSON.stringify(payload, null, 2)
  if (window.showSaveFilePicker) {
    const handle = await window.showSaveFilePicker({
      suggestedName: filename,
      types: [
        {
          description: 'Monitoring Project JSON',
          accept: { 'application/json': ['.json'] }
        }
      ]
    })
    const writable = await handle.createWritable()
    await writable.write(json)
    await writable.close()
    return
  }

  const blob = new Blob([json], { type: 'application/json;charset=utf-8' })
  const url = URL.createObjectURL(blob)
  const a = document.createElement('a')
  a.href = url
  a.download = filename
  document.body.appendChild(a)
  a.click()
  a.remove()
  URL.revokeObjectURL(url)
}

async function exportProject(project) {
  try {
    await saveJsonFile(`${safeFileName(project.name)}.json`, exportPayload(project))
    fileMessage.value = `已导出「${project.name}」`
  } catch (e) {
    if (e?.name !== 'AbortError') fileMessage.value = `导出失败：${e.message || e}`
  }
}

async function exportCurrentLayout() {
  await exportProject({
    name: projectStore.currentProject?.name || '当前画布',
    description: '从当前画布导出的项目文件',
    layout: props.currentLayout
  })
}

function triggerImport() {
  fileMessage.value = ''
  importInput.value?.click()
}

function pickImportedProject(data, fallbackName) {
  const normalized = normalizeScadaProjectImport(data, fallbackName)
  const stamp = new Date()
    .toLocaleString('zh-CN', { month: '2-digit', day: '2-digit', hour: '2-digit', minute: '2-digit', hour12: false })
    .replace(/[^\d]/g, '')
  const baseName = String(normalized.name || fallbackName || '导入项目').trim().slice(0, 26)
  return {
    name: `${baseName} 导入${stamp}`.slice(0, 40),
    description: normalized.description,
    layout: normalized.layout,
    customComponents: normalized.customComponents,
    format: normalized.format
  }
}

async function importProjectFile(event) {
  const file = event.target.files?.[0]
  event.target.value = ''
  if (!file) return
  try {
    if (file.size > 50 * 1024 * 1024) throw new Error('项目 JSON 超过 50 MB，请精简资源后导入')
    const text = await file.text()
    const data = JSON.parse(text)
    const payload = pickImportedProject(data, file.name.replace(/\.json$/i, ''))
    const checked = validateImportedProjectBindings(payload.layout, data.variableReferences, monitorStore.variables)
    payload.layout = checked.layout
    await projectStore.createProject(payload)
    let componentMessage = ''
    if (payload.customComponents.length) {
      try {
        const count = await monitorStore.importCustomComponentsFromServer(payload.customComponents)
        componentMessage = `，合并 ${count} 个自定义组件`
      } catch {
        componentMessage = '；自定义组件库未能合并，请在组件设计器中单独导入'
      }
    }
    fileMessage.value = `已从 ${payload.format} 导入为「${payload.name}」${componentMessage}${checked.unresolved ? `；${checked.unresolved} 处变量引用需要重新绑定` : ''}`
  } catch (e) {
    fileMessage.value = `导入失败：${e.message || e}`
  }
}

function loadProject(project) {
  try {
    monitorStore.activateProject(project)
    projectStore.setCurrentProject(project)
    emit('load-layout', monitorStore.layoutSnapshot)
    emit('close')
  } catch (error) {
    fileMessage.value = error.message || '项目加载失败'
  }
}

function changePage(page) {
  projectStore.setQuery({ page })
}

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
.dialog-project-manager {
  width: min(1120px, 96vw);
  max-width: 96vw;
  padding: 0;
  overflow: hidden;
}
.pm-header {
  display: flex;
  align-items: flex-start;
  justify-content: space-between;
  gap: 12px;
  padding: 22px 24px 16px;
  border-bottom: 1px solid var(--border-light);
  background: var(--gradient-pane);
}
.pm-header h3 {
  margin: 0;
}
.pm-header p {
  margin-top: 4px;
  color: var(--text-muted);
  font-size: var(--fs-sm);
}
.pm-toolbar {
  display: flex;
  align-items: center;
  justify-content: space-between;
  flex-wrap: wrap;
  gap: 10px;
  padding: 16px 24px;
  border-bottom: 1px solid var(--border-light);
  background: var(--bg-primary);
}
.pm-filter-group,
.pm-action-group {
  display: flex;
  align-items: center;
  gap: 10px;
  flex-wrap: wrap;
  min-width: 0;
}
.pm-filter-group {
  flex: 1 1 520px;
}
.pm-action-group {
  flex: 0 1 auto;
  justify-content: flex-end;
}
.pm-filter-group input {
  flex: 1 1 220px;
  min-width: 180px;
}
.pm-filter-group select {
  flex: 0 0 150px;
}
.pm-filter-group select:last-child {
  flex-basis: 120px;
}
.pm-action-group .btn {
  white-space: nowrap;
}
.pm-file-input {
  display: none;
}
.pm-error {
  margin: 14px 24px 0;
  padding: 10px 12px;
  border-radius: var(--radius-md);
  border: 1px solid var(--danger-border);
  background: var(--danger-bg);
  color: var(--danger-text);
  font-size: var(--fs-sm);
}
.pm-note {
  margin: 14px 24px 0;
  padding: 10px 12px;
  border-radius: var(--radius-md);
  border: 1px solid var(--border-light);
  background: var(--surface-muted);
  color: var(--text-secondary);
  font-size: var(--fs-sm);
}
.pm-list {
  display: flex;
  flex-direction: column;
  gap: 10px;
  min-height: 300px;
  max-height: 52vh;
  overflow-y: auto;
  padding: 16px 24px;
  background: var(--bg-secondary);
}
.pm-empty {
  display: flex;
  align-items: center;
  justify-content: center;
  min-height: 220px;
  border: 1px dashed var(--border-color);
  border-radius: var(--radius-lg);
  color: var(--text-placeholder);
  background: var(--bg-primary);
  font-size: var(--fs-sm);
}
.pm-footer {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 12px;
  padding: 14px 24px;
  border-top: 1px solid var(--border-light);
  color: var(--text-tertiary);
  font-size: var(--fs-xs);
}
.pm-pages {
  display: flex;
  align-items: center;
  gap: 8px;
}
.pm-toast {
  position: absolute;
  left: 24px;
  bottom: 18px;
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 10px 12px;
  border-radius: var(--radius-md);
  background: rgba(23, 32, 51, 0.94);
  color: var(--text-inverse);
  box-shadow: var(--shadow-lg);
  font-size: var(--fs-sm);
}
.pm-toast-copy {
  left: auto;
  right: 24px;
}
.project-detail-dialog {
  width: 460px;
}
.detail-grid {
  display: grid;
  grid-template-columns: 88px minmax(0, 1fr);
  gap: 10px 12px;
  font-size: var(--fs-sm);
}
.detail-grid span {
  color: var(--text-tertiary);
}
.detail-grid strong {
  min-width: 0;
  overflow-wrap: anywhere;
  color: var(--text-primary);
}

@media (max-width: 760px) {
  .pm-toolbar {
    grid-template-columns: 1fr;
  }
  .pm-footer {
    align-items: flex-start;
    flex-direction: column;
  }
  .pm-toast {
    left: 12px;
    right: 12px;
    bottom: 12px;
  }
  .pm-toast-copy {
    bottom: 62px;
  }
}
</style>
