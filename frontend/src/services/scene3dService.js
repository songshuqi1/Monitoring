// Portable, renderer-independent scene format. Runtime measurements never enter this file.
export const SCENE_VERSION = 1
export const MODEL_LIBRARY = [
  { type: 'tank', name: '立式储罐', icon: '▥', color: '#8da5b5' },
  { type: 'pump', name: '离心水泵', icon: '◉', color: '#397ca7' },
  { type: 'valve', name: '工业阀门', icon: '⋈', color: '#478b85' },
  { type: 'motor', name: '电动机', icon: '▤', color: '#497d9c' },
  { type: 'fan', name: '工业风机', icon: '✣', color: '#7393a5' },
  { type: 'pipe', name: '工艺管道', icon: '━', color: '#52a4a0' },
  { type: 'building', name: '厂房框架', icon: '⌂', color: '#aab7c2' },
  { type: 'platform', name: '设备平台', icon: '▱', color: '#939eac' }
]
const types = new Set(MODEL_LIBRARY.map(item => item.type))
const number = (v, fallback, min = -10000, max = 10000) => {
  if (v === null || v === '' || v === undefined) return fallback
  const n = Number(v)
  return Number.isFinite(n) ? Math.max(min, Math.min(max, n)) : fallback
}
const vector = (v, fallback, min = -10000, max = 10000) => fallback.map((n, i) => number(v?.[i], n, min, max))
export const copyScene = value => JSON.parse(JSON.stringify(value))
export const sceneId = () => `n-${globalThis.crypto?.randomUUID?.() || `${Date.now()}-${Math.random().toString(36).slice(2)}`}`

export function emptyScene() {
  return {
    version: SCENE_VERSION, nodes: [],
    environment: { theme: 'light', grid: true, labels: true, labelFontSize: 14, quality: 'standard' },
    camera: { position: [18, 15, 20], target: [0, 1, 0] }
  }
}

export function createSceneNode(type, patch = {}) {
  return normalizeNode({ id: sceneId(), type, name: MODEL_LIBRARY.find(m => m.type === type)?.name || '设备', ...patch })
}

function normalizeNode(node) {
  if (!node || !types.has(node.type)) throw new Error(`不支持的 3D 组件类型：${node?.type || '空'}`)
  const bindings = {}
  for (const key of ['status', 'value', 'fault']) {
    const binding = node.bindings?.[key]
    bindings[key] = binding == null || binding === '' ? null : String(binding)
  }
  return {
    id: String(node.id || sceneId()), type: node.type,
    name: String(node.name || MODEL_LIBRARY.find(m => m.type === node.type).name).slice(0, 100),
    labelFontSize: node.labelFontSize == null ? null : number(node.labelFontSize, 14, 8, 48),
    deviceId: String(node.deviceId || '').slice(0, 100),
    linkedWidgetId: node.linkedWidgetId ? String(node.linkedWidgetId) : null,
    position: vector(node.position, [0, 0, 0]),
    rotation: vector(node.rotation, [0, 0, 0], -360, 360),
    scale: vector(node.scale, [1, 1, 1], 0.05, 100),
    color: /^#[0-9a-f]{6}$/i.test(node.color) ? node.color : MODEL_LIBRARY.find(m => m.type === node.type).color,
    visible: node.visible !== false, locked: node.locked === true,
    bindings,
    range: { min: number(node.range?.min, 0), max: number(node.range?.max, 100) },
    staleAfterMs: number(node.staleAfterMs, 15000, 1000, 3600000),
    pipeLength: number(node.pipeLength, 4, 0.2, 200)
  }
}

export function normalizeScene(payload) {
  if (payload == null) return emptyScene()
  if (typeof payload !== 'object' || Array.isArray(payload)) throw new Error('3D 场景格式无效')
  if (payload.version != null && Number(payload.version) !== SCENE_VERSION) throw new Error('不支持此 3D 场景版本，请使用兼容版本的平台')
  if (!Array.isArray(payload.nodes)) throw new Error('3D 场景缺少设备列表 nodes')
  if (payload.nodes.length > 1000) throw new Error('单个 3D 场景最多支持 1000 个组件')
  const nodes = payload.nodes.map(normalizeNode)
  const ids = new Set()
  for (const node of nodes) {
    if (ids.has(node.id)) throw new Error(`3D 设备编号重复：${node.id}`)
    ids.add(node.id)
    if (node.range.max <= node.range.min) throw new Error(`「${node.name}」的量程上限必须大于下限`)
  }
  const defaults = emptyScene()
  return {
    version: SCENE_VERSION, nodes,
    environment: {
      theme: payload.environment?.theme === 'dark' ? 'dark' : 'light',
      grid: payload.environment?.grid !== false, labels: payload.environment?.labels !== false,
      labelFontSize: number(payload.environment?.labelFontSize, 14, 8, 48),
      quality: ['smooth', 'standard', 'fine'].includes(payload.environment?.quality) ? payload.environment.quality : 'standard'
    },
    camera: {
      position: vector(payload.camera?.position, defaults.camera.position),
      target: vector(payload.camera?.target, defaults.camera.target)
    }
  }
}

export function effectiveBindings(node, widgets = []) {
  const result = { ...node.bindings }
  if (node.linkedWidgetId) {
    const widget = widgets.find(w => String(w.id) === node.linkedWidgetId)
    // Following a 2D widget must not silently fall back to a previous device's variable.
    result[node.type === 'tank' ? 'value' : 'status'] = widget?.config?.varId == null ? null : String(widget.config.varId)
  }
  return result
}

export function nodeRuntime(node, store, now = Date.now()) {
  const bindings = effectiveBindings(node, store.widgets)
  const ids = Object.values(bindings).filter(id => id != null && id !== '')
  const variableById = new Map(store.variables.map(v => [String(v.id), v]))
  const point = id => id == null ? null : store.realtimeData[id]
  let state = 'unbound'
  const missingLink = node.linkedWidgetId && !store.widgets.some(w => String(w.id) === node.linkedWidgetId)
  if (missingLink || ids.some(id => !variableById.has(String(id)))) state = 'missing'
  else if (ids.length) {
    if (ids.some(id => !point(id) || point(id).quality !== 'GOOD' || point(id).value == null || !Number.isFinite(Number(point(id).value)))) state = 'offline'
    else if (ids.some(id => {
      const timestamp = Number(point(id).timestamp ?? point(id).timestampMs)
      return !Number.isFinite(timestamp) || timestamp <= 0 || now - timestamp > node.staleAfterMs
    })) state = 'stale'
    else if (bindings.fault && Number(point(bindings.fault)?.value) !== 0) state = 'fault'
    else if (bindings.status) state = Number(point(bindings.status)?.value) > 0 ? 'running' : 'stopped'
    else state = 'good'
  }
  const valid = ['good', 'running', 'stopped', 'fault'].includes(state)
  const valueId = bindings.value || bindings.status
  const raw = point(valueId)?.value
  const value = valid && raw != null && Number.isFinite(Number(raw)) ? Number(raw) : null
  const unit = variableById.get(String(valueId))?.unit || ''
  const labels = { unbound: '待绑定', missing: '绑定缺失', offline: '离线 / 质量异常', stale: '数据过期', fault: '故障', running: '运行', stopped: '停止', good: '正常' }
  return {
    state, label: labels[state], value, unit, running: state === 'running',
    level: valid && bindings.value && value != null ? Math.max(0, Math.min(1, (value - node.range.min) / (node.range.max - node.range.min))) : 0,
    text: value == null ? '—' : `${Number(value.toFixed(2))}${unit ? ` ${unit}` : ''}`
  }
}

export function sceneExport(scene, name, variables, widgets = []) {
  const normalized = normalizeScene(scene)
  // A standalone scene cannot depend on widgets which are not in the exported file.
  normalized.nodes = normalized.nodes.map(node => ({ ...node, bindings: effectiveBindings(node, widgets), linkedWidgetId: null }))
  const ids = new Set(normalized.nodes.flatMap(node => Object.values(node.bindings)).filter(Boolean))
  return {
    type: 'monitoring-platform-scene3d', version: 1, name, exportedAt: new Date().toISOString(), scene3d: normalized,
    variableReferences: variables.filter(v => ids.has(String(v.id))).map(variableReference)
  }
}

export function parseSceneFile(payload, variables = []) {
  if (payload?.type !== 'monitoring-platform-scene3d' || payload.version !== 1) throw new Error('请选择由本平台导出的 3D 场景 JSON；完整项目请从项目管理导入')
  const scene = normalizeScene(payload.scene3d)
  const refs = Array.isArray(payload.variableReferences) ? payload.variableReferences : []
  let unresolved = 0
  for (const node of scene.nodes) {
    node.linkedWidgetId = null
    for (const key of ['status', 'value', 'fault']) {
      const id = node.bindings[key]
      if (!id) continue
      const hint = refs.find(v => String(v.id) === id)
      const match = hint && variables.find(v => referenceMatches(v, hint))
      if (!match) { node.bindings[key] = `unresolved:${id}`; unresolved++ }
    }
  }
  return { scene, unresolved }
}

function variableReference(v) {
  return { id: String(v.id), name: v.name, source: v.source, resourceId: v.resourceId ?? null, dataType: v.dataType ?? null, opcuaNodeId: v.opcuaNodeId ?? null, scopeId: v.scopeId ?? null }
}
function referenceMatches(variable, reference) {
  return ['id', 'name', 'source', 'resourceId', 'dataType', 'opcuaNodeId', 'scopeId'].every(key => String(variable[key] ?? '') === String(reference[key] ?? ''))
}
export function projectVariableReferences(layout, variables) {
  const ids = new Set()
  for (const widget of layout.widgets || []) {
    if (widget.config?.varId != null) ids.add(String(widget.config.varId))
    for (const id of widget.config?.varIds || []) ids.add(String(id))
  }
  for (const node of layout.scene3d?.nodes || []) {
    for (const id of Object.values(effectiveBindings(node, layout.widgets))) if (id != null) ids.add(String(id))
  }
  return variables.filter(v => ids.has(String(v.id))).map(variableReference)
}
export function validateImportedProjectBindings(layout, references, variables) {
  // Legacy files do not carry reference metadata. Preserve their established import behavior.
  if (!Array.isArray(references)) return { layout, unresolved: 0 }
  const next = copyScene(layout)
  let unresolved = 0
  const resolve = id => {
    if (id == null || id === '') return id
    const hint = references.find(v => String(v.id) === String(id))
    if (hint && variables.some(v => referenceMatches(v, hint))) return id
    unresolved++
    return `unresolved:${id}`
  }
  for (const widget of next.widgets || []) {
    if (widget.config?.varId != null) widget.config.varId = resolve(widget.config.varId)
    if (Array.isArray(widget.config?.varIds)) widget.config.varIds = widget.config.varIds.map(resolve)
  }
  for (const node of next.scene3d?.nodes || []) {
    for (const key of ['status', 'value', 'fault']) node.bindings[key] = resolve(node.bindings[key])
  }
  return { layout: next, unresolved }
}

export function downloadJson(filename, payload) {
  const url = URL.createObjectURL(new Blob([JSON.stringify(payload, null, 2)], { type: 'application/json;charset=utf-8' }))
  const a = document.createElement('a')
  a.href = url
  a.download = `${String(filename || '3D场景').replace(/[\\/:*?"<>|]/g, '_')}.json`
  document.body.appendChild(a)
  a.click()
  a.remove()
  setTimeout(() => URL.revokeObjectURL(url), 1000)
}
