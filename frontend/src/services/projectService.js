import { normalizeScene } from './scene3dService.js'

const PROJECT_KEY = 'monitor-projects'

function delayResult(value) {
  return Promise.resolve(value)
}

function now() {
  return Date.now()
}

function clone(value) {
  return JSON.parse(JSON.stringify(value))
}

function normalizeLayout(layout) {
  if (Array.isArray(layout)) {
    return { widgets: layout, connections: [] }
  }
  if (layout && typeof layout === 'object') {
    return {
      widgets: Array.isArray(layout.widgets) ? layout.widgets : [],
      connections: Array.isArray(layout.connections) ? layout.connections : [],
      scene3d: normalizeScene(layout.scene3d)
    }
  }
  return { widgets: [], connections: [] }
}

function createId() {
  return 'proj-' + now() + '-' + Math.random().toString(36).slice(2, 8)
}

function readAll() {
  try {
    const raw = localStorage.getItem(PROJECT_KEY)
    const parsed = raw ? JSON.parse(raw) : []
    return parsed.map(normalizeProject)
  } catch (error) {
    throw new Error(`项目存储读取失败，原始数据已保留：${error.message}`)
  }
}

function writeAll(projects) {
  localStorage.setItem(PROJECT_KEY, JSON.stringify(projects))
}

function normalizeProject(project) {
  const ts = project.updatedAt || project.createdAt || now()
  return {
    id: project.id || createId(),
    name: project.name || '未命名项目',
    description: project.description || '',
    layout: normalizeLayout(project.layout),
    createdAt: project.createdAt || ts,
    updatedAt: project.updatedAt || ts,
    deletedAt: project.deletedAt || null
  }
}

function assertValidPayload(payload, options = {}) {
  const name = (payload.name || '').trim()
  if (!name) throw Object.assign(new Error('请输入项目名称'), { code: 'VALIDATION_NAME_REQUIRED' })
  if (name.length > 40) throw Object.assign(new Error('项目名称不能超过 40 个字符'), { code: 'VALIDATION_NAME_LENGTH' })
  if ((payload.description || '').length > 160) {
    throw Object.assign(new Error('项目描述不能超过 160 个字符'), { code: 'VALIDATION_DESCRIPTION_LENGTH' })
  }

  const projects = readAll()
  const duplicate = projects.find(p =>
    !p.deletedAt &&
    p.name.trim().toLowerCase() === name.toLowerCase() &&
    p.id !== options.ignoreId
  )
  if (duplicate) throw Object.assign(new Error('项目名称已存在'), { code: 'DUPLICATE_PROJECT_NAME' })
}

function sortProjects(projects, sortBy, sortDir) {
  const dir = sortDir === 'asc' ? 1 : -1
  return [...projects].sort((a, b) => {
    const av = a[sortBy] ?? ''
    const bv = b[sortBy] ?? ''
    if (typeof av === 'number' && typeof bv === 'number') return (av - bv) * dir
    return String(av).localeCompare(String(bv), 'zh-CN') * dir
  })
}

function uniqueCopyName(projects, baseName) {
  const base = `${baseName || '未命名项目'} 副本`
  const names = new Set(projects.filter(p => !p.deletedAt).map(p => p.name.trim().toLowerCase()))
  if (!names.has(base.toLowerCase())) return base

  let index = 2
  while (names.has(`${base} ${index}`.toLowerCase())) index++
  return `${base} ${index}`
}

export const projectService = {
  async listProjects(query = {}) {
    const {
      page = 1,
      pageSize = 8,
      search = '',
      sortBy = 'updatedAt',
      sortDir = 'desc',
      includeDeleted = false
    } = query

    const keyword = search.trim().toLowerCase()
    let items = readAll().filter(p => includeDeleted ? true : !p.deletedAt)
    if (keyword) {
      items = items.filter(p =>
        p.name.toLowerCase().includes(keyword) ||
        p.description.toLowerCase().includes(keyword)
      )
    }
    items = sortProjects(items, sortBy, sortDir)

    const total = items.length
    const safePageSize = Math.max(1, Number(pageSize) || 8)
    const totalPages = Math.max(1, Math.ceil(total / safePageSize))
    const safePage = Math.min(Math.max(1, Number(page) || 1), totalPages)
    const start = (safePage - 1) * safePageSize

    return delayResult({
      items: items.slice(start, start + safePageSize),
      total,
      page: safePage,
      pageSize: safePageSize,
      totalPages
    })
  },

  async createProject(payload) {
    assertValidPayload(payload)
    const projects = readAll()
    const time = now()
    const project = normalizeProject({
      id: createId(),
      name: payload.name.trim(),
      description: (payload.description || '').trim(),
      layout: clone(payload.layout || []),
      createdAt: time,
      updatedAt: time
    })
    projects.push(project)
    writeAll(projects)
    return delayResult(project)
  },

  async updateProject(id, payload) {
    assertValidPayload(payload, { ignoreId: id })
    const projects = readAll()
    const index = projects.findIndex(p => p.id === id)
    if (index < 0) throw Object.assign(new Error('项目不存在或已被删除'), { code: 'PROJECT_NOT_FOUND' })
    const current = projects[index]
    projects[index] = normalizeProject({
      ...current,
      name: payload.name.trim(),
      description: (payload.description || '').trim(),
      layout: clone(payload.layout ?? current.layout),
      updatedAt: now(),
      deletedAt: null
    })
    writeAll(projects)
    return delayResult(projects[index])
  },

  async duplicateProject(id) {
    const projects = readAll()
    const source = projects.find(p => p.id === id)
    if (!source || source.deletedAt) {
      throw Object.assign(new Error('项目不存在或已被删除'), { code: 'PROJECT_NOT_FOUND' })
    }

    const time = now()
    const project = normalizeProject({
      id: createId(),
      name: uniqueCopyName(projects, source.name),
      description: source.description,
      layout: clone(source.layout || []),
      createdAt: time,
      updatedAt: time,
      deletedAt: null
    })
    projects.push(project)
    writeAll(projects)
    return delayResult(project)
  },

  async softDeleteProject(id) {
    const projects = readAll()
    const index = projects.findIndex(p => p.id === id)
    if (index < 0) throw Object.assign(new Error('项目不存在'), { code: 'PROJECT_NOT_FOUND' })
    projects[index] = { ...projects[index], deletedAt: now(), updatedAt: now() }
    writeAll(projects)
    return delayResult(projects[index])
  },

  async restoreProject(id) {
    const projects = readAll()
    const index = projects.findIndex(p => p.id === id)
    if (index < 0) throw Object.assign(new Error('项目不存在'), { code: 'PROJECT_NOT_FOUND' })
    projects[index] = { ...projects[index], deletedAt: null, updatedAt: now() }
    writeAll(projects)
    return delayResult(projects[index])
  },

  async getProject(id) {
    const project = readAll().find(p => p.id === id)
    if (!project) throw Object.assign(new Error('项目不存在'), { code: 'PROJECT_NOT_FOUND' })
    return delayResult(project)
  }
}
