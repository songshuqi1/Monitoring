import { defineStore } from 'pinia'
import { computed, reactive, ref, watch } from 'vue'
import { useRequest } from '../composables/useRequest.js'
import { projectService } from '../services/projectService.js'

const DEFAULT_QUERY = {
  page: 1,
  pageSize: 8,
  search: '',
  sortBy: 'updatedAt',
  sortDir: 'desc',
  includeDeleted: false
}

const CURRENT_PROJECT_KEY = 'monitor-current-project-id'

export const useProjectStore = defineStore('projects', () => {
  const query = reactive({ ...DEFAULT_QUERY })
  const list = ref([])
  const total = ref(0)
  const totalPages = ref(1)
  const currentProject = ref(null)
  const undoDeleteProject = ref(null)
  const userRole = ref(localStorage.getItem('monitor-user-role') || 'admin')

  const listRequest = useRequest((params) => projectService.listProjects(params))
  const mutationRequest = useRequest((action) => action())

  const loading = computed(() => listRequest.loading.value || mutationRequest.loading.value)
  const errorMessage = computed(() => listRequest.errorMessage.value || mutationRequest.errorMessage.value)
  const canDelete = computed(() => userRole.value === 'admin')

  function loadQueryFromUrl() {
    const params = new URLSearchParams(window.location.search)
    query.page = Number(params.get('projectPage')) || DEFAULT_QUERY.page
    query.pageSize = Number(params.get('projectPageSize')) || DEFAULT_QUERY.pageSize
    query.search = params.get('projectSearch') || DEFAULT_QUERY.search
    query.sortBy = params.get('projectSortBy') || DEFAULT_QUERY.sortBy
    query.sortDir = params.get('projectSortDir') || DEFAULT_QUERY.sortDir
  }

  function syncQueryToUrl() {
    const params = new URLSearchParams(window.location.search)
    const pairs = {
      projectPage: query.page,
      projectPageSize: query.pageSize,
      projectSearch: query.search,
      projectSortBy: query.sortBy,
      projectSortDir: query.sortDir
    }
    Object.entries(pairs).forEach(([key, value]) => {
      if (value === DEFAULT_QUERY[key.replace('project', '').replace(/^./, c => c.toLowerCase())] || value === '') {
        params.delete(key)
      } else {
        params.set(key, String(value))
      }
    })
    const next = params.toString()
    const url = next ? `${window.location.pathname}?${next}` : window.location.pathname
    window.history.replaceState(null, '', url)
  }

  async function fetchProjects() {
    const result = await listRequest.run({ ...query })
    list.value = result.items
    total.value = result.total
    totalPages.value = result.totalPages
    query.page = result.page
    query.pageSize = result.pageSize
    syncQueryToUrl()
    return result
  }

  function setQuery(patch) {
    Object.assign(query, patch)
    if (!Object.prototype.hasOwnProperty.call(patch, 'page')) query.page = 1
  }

  async function createProject(payload) {
    const project = await mutationRequest.run(() => projectService.createProject(payload))
    await fetchProjects()
    return project
  }

  async function updateProject(id, payload) {
    const project = await mutationRequest.run(() => projectService.updateProject(id, payload))
    if (currentProject.value?.id === id) setCurrentProject(project)
    await fetchProjects()
    return project
  }

  async function duplicateProject(project) {
    const copied = await mutationRequest.run(() => projectService.duplicateProject(project.id))
    await fetchProjects()
    return copied
  }

  async function softDeleteProject(project) {
    if (!canDelete.value) throw new Error('当前用户没有删除项目权限')
    const deleted = await mutationRequest.run(() => projectService.softDeleteProject(project.id))
    if (currentProject.value?.id === project.id) clearCurrentProject()
    undoDeleteProject.value = deleted
    await fetchProjects()
    return deleted
  }

  async function restoreLastDeleted() {
    if (!undoDeleteProject.value) return null
    const restored = await mutationRequest.run(() => projectService.restoreProject(undoDeleteProject.value.id))
    undoDeleteProject.value = null
    await fetchProjects()
    return restored
  }

  async function selectProject(id) {
    const project = await mutationRequest.run(() => projectService.getProject(id))
    setCurrentProject(project)
    return project
  }

  function setCurrentProject(project) {
    currentProject.value = project || null
    if (project?.id) localStorage.setItem(CURRENT_PROJECT_KEY, project.id)
    else localStorage.removeItem(CURRENT_PROJECT_KEY)
  }

  function clearCurrentProject() {
    setCurrentProject(null)
  }

  async function restoreCurrentProject() {
    const id = localStorage.getItem(CURRENT_PROJECT_KEY)
    if (!id) return null
    try {
      const project = await projectService.getProject(id)
      if (project.deletedAt) {
        clearCurrentProject()
        return null
      }
      setCurrentProject(project)
      return project
    } catch {
      clearCurrentProject()
      return null
    }
  }

  function clearUndo() {
    undoDeleteProject.value = null
  }

  function setUserRole(role) {
    userRole.value = role
    localStorage.setItem('monitor-user-role', role)
  }

  watch(() => ({ ...query }), () => {
    fetchProjects().catch(() => {})
  }, { deep: true })

  return {
    query,
    list,
    total,
    totalPages,
    currentProject,
    undoDeleteProject,
    userRole,
    loading,
    errorMessage,
    canDelete,
    loadQueryFromUrl,
    fetchProjects,
    setQuery,
    createProject,
    updateProject,
    duplicateProject,
    softDeleteProject,
    restoreLastDeleted,
    selectProject,
    setCurrentProject,
    clearCurrentProject,
    restoreCurrentProject,
    clearUndo,
    setUserRole
  }
})
