// Compatibility importer for Monitoring-T projects and legacy SCADA/X6 graph JSON.
// It intentionally produces only normal Monitoring-T layout data, so imported files
// remain editable without a Neuiai backend or asset service.

const isObject = value => value && typeof value === 'object' && !Array.isArray(value)
const asJson = value => {
  if (typeof value !== 'string') return value
  try { return JSON.parse(value) } catch { return value }
}
const finite = (value, fallback) => Number.isFinite(Number(value)) ? Number(value) : fallback
const nested = (source, path) => String(path).split('.').reduce((value, key) => value == null ? undefined : value[key], source)

function walk(value, visitor, depth = 0, seen = new Set()) {
  if (depth > 8) return null
  const current = asJson(value)
  const result = visitor(current)
  if (result) return result
  if (Array.isArray(current)) {
    for (const item of current) {
      const found = walk(item, visitor, depth + 1, seen)
      if (found) return found
    }
    return null
  }
  if (!isObject(current) || seen.has(current)) return null
  seen.add(current)
  const priority = ['monitoring', 'project', 'layout', 'graph', 'graphJson', 'graph_json', 'x6', 'canvas', 'data', 'content', 'config']
  const keys = [...priority.filter(key => Object.hasOwn(current, key)), ...Object.keys(current).filter(key => !priority.includes(key))]
  for (const key of keys) {
    const found = walk(current[key], visitor, depth + 1, seen)
    if (found) return found
  }
  return null
}

function hasLayout(value) {
  return isObject(value) && (Array.isArray(value.widgets) || Array.isArray(value.connections))
}

function findLayout(value) {
  return walk(value, current => {
    if (hasLayout(current)) return current
    if (hasLayout(current?.layout)) return current.layout
    if (hasLayout(current?.project?.layout)) return current.project.layout
    if (Array.isArray(current) && current.some(item => isObject(item) && typeof item.type === 'string')) {
      return { widgets: current, connections: [] }
    }
    return null
  })
}

function findCustomComponents(value) {
  return walk(value, current => Array.isArray(current?.customComponents) ? current.customComponents : null)
}

function findGraph(value) {
  return walk(value, current => {
    if (Array.isArray(current?.cells)) return current
    const candidate = asJson(current?.graph || current?.graphJson || current?.graph_json || current?.x6)
    if (Array.isArray(candidate?.cells)) return candidate
    if (Array.isArray(candidate) && candidate.some(cell => isObject(cell) && (cell.shape || cell.source || cell.target))) return { cells: candidate }
    return null
  })
}

function textForCell(cell, index) {
  return String(
    cell?.data?.name || cell?.data?.variableName ||
    nested(cell, 'attrs.label.text') || nested(cell, 'attrs.text.text') ||
    `SCADA组件${index + 1}`
  )
}

function imagePath(cell) {
  const candidates = [nested(cell, 'attrs.image.xlink:href'), nested(cell, 'attrs.image.href'), cell?.data?.path, cell?.data?.url, cell?.path, cell?.url]
  return candidates.find(value => typeof value === 'string' && value.trim()) || ''
}

function geometry(cell) {
  const shape = String(cell?.shape || '').toLowerCase()
  const points = nested(cell, 'attrs.body.points')
  if (points) return `M${String(points).trim().split(/\s+/).join('L')}Z`
  if (shape === 'circle') return 'M40 4A36 36 0 1 1 39.99 4Z'
  if (shape === 'ellipse') return 'M4 40A36 26 0 1 1 76 40A36 26 0 1 1 4 40Z'
  if (shape === 'polygon' || String(cell?.data?.id || '').toLowerCase() === 'triangle') return 'M40 8L74 72H6Z'
  return ''
}

function color(value, fallback) {
  return /^#[0-9a-f]{3,8}$/i.test(String(value || '')) ? String(value) : fallback
}

function widgetId(cell, index) {
  return `legacy-${String(cell?.id || `cell-${index + 1}`).replace(/[^a-zA-Z0-9_-]/g, '-')}-${index + 1}`
}

function cellPosition(cell) {
  return { x: finite(cell?.position?.x ?? cell?.x, 40), y: finite(cell?.position?.y ?? cell?.y, 40) }
}

function cellSize(cell, fallbackW = 180, fallbackH = 100) {
  return {
    w: Math.max(1, finite(cell?.size?.width ?? cell?.width, fallbackW)),
    h: Math.max(1, finite(cell?.size?.height ?? cell?.height, fallbackH))
  }
}

function nodeToWidget(cell, index) {
  const { x, y } = cellPosition(cell)
  const { w, h } = cellSize(cell)
  const title = textForCell(cell, index)
  const dataType = String(cell?.data?.type || '').toLowerCase()
  const shape = String(cell?.shape || '').toLowerCase()
  const path = imagePath(cell)
  if (path || geometry(cell)) {
    return {
      id: widgetId(cell, index), type: 'scadaSvg', x, y, w, h,
      config: {
        title, label: title, labelFontSize: 12, asset: path
          ? { id: `legacy-asset-${index + 1}`, name: title, type: 'image', path }
          : { id: `legacy-geometry-${index + 1}`, name: title, type: 'geometry', path: geometry(cell) },
        preserveAspectRatio: 'meet', bindVariable: false, varId: null
      }
    }
  }
  if (dataType === 'data-field' || cell?.data?.source || cell?.data?.source_type) {
    return { id: widgetId(cell, index), type: 'digitalDisplay', x, y, w, h, config: { title, varId: null, unit: cell?.data?.unit || '', decimals: 1, min: 0, max: 100 } }
  }
  if (dataType === 'text' || (shape && !['rect', 'rectangle', 'image'].includes(shape))) {
    return { id: widgetId(cell, index), type: 'label', x, y, w, h, config: { title, text: title, fontSize: finite(nested(cell, 'attrs.label.fontSize'), 14), color: color(nested(cell, 'attrs.label.fill'), '#344054'), align: 'center', verticalAlign: 'center' } }
  }
  return {
    id: widgetId(cell, index), type: 'frameBox', x, y, w, h,
    config: { title, background: color(nested(cell, 'attrs.body.fill'), '#ffffff'), backgroundOpacity: 0, borderColor: color(nested(cell, 'attrs.body.stroke'), '#4f6fb8'), borderWidth: Math.max(0, finite(nested(cell, 'attrs.body.strokeWidth'), 2)), borderRadius: Math.max(0, finite(nested(cell, 'attrs.body.rx'), 0)) }
  }
}

function endpointId(endpoint) {
  if (typeof endpoint === 'string') return endpoint
  return endpoint?.cell || endpoint?.id || null
}

function endpointPoint(endpoint) {
  const x = finite(endpoint?.x, NaN)
  const y = finite(endpoint?.y, NaN)
  return Number.isFinite(x) && Number.isFinite(y) ? { x, y } : null
}

function graphToLayout(graph) {
  const cells = Array.isArray(graph?.cells) ? graph.cells : []
  const widgets = []
  const connections = []
  const idMap = new Map()
  cells.forEach((cell, index) => {
    if (cell?.source || cell?.target) return
    const widget = nodeToWidget(cell, index)
    widgets.push(widget)
    idMap.set(String(cell?.id || `cell-${index + 1}`), widget.id)
  })
  cells.forEach((cell, index) => {
    if (!cell?.source && !cell?.target) return
    const source = idMap.get(String(endpointId(cell.source))) || null
    const target = idMap.get(String(endpointId(cell.target))) || null
    const vertices = Array.isArray(cell?.vertices) ? cell.vertices.map(endpointPoint).filter(Boolean) : []
    const base = { id: `legacy-line-${index + 1}`, color: color(nested(cell, 'attrs.line.stroke'), '#178447'), width: Math.max(1, finite(nested(cell, 'attrs.line.strokeWidth'), 2)), startArrow: Boolean(nested(cell, 'attrs.line.sourceMarker')), endArrow: Boolean(nested(cell, 'attrs.line.targetMarker')), kind: 'pipe' }
    if (source && target) connections.push({ ...base, from: source, to: target, waypoints: vertices.length ? vertices : null })
    else {
      const points = [endpointPoint(cell.source), ...vertices, endpointPoint(cell.target)].filter(Boolean)
      if (points.length >= 2) connections.push({ ...base, from: null, to: null, points })
    }
  })
  return { widgets, connections }
}

function isLegacyWhite(value) {
  const normalized = String(value ?? '').trim().toLowerCase().replace(/\s+/g, '')
  return !normalized || [
    '#fff', '#ffffff', '#ffffffff', 'white',
    'rgb(255,255,255)', 'rgba(255,255,255,1)'
  ].includes(normalized)
}

// Earlier Monitoring-T exports could persist label defaults as an opaque white
// panel.  Labels created by the current editor are transparent, so normalize
// only the old white/default values during import while preserving deliberate
// non-white label backgrounds.
function normalizeImportedLayout(layout) {
  if (!isObject(layout)) return { widgets: [], connections: [] }
  const widgets = Array.isArray(layout.widgets) ? layout.widgets.map(widget => {
    if (!isObject(widget)) return widget
    if (String(widget.type || '').toLowerCase() !== 'label') return { ...widget }

    const config = isObject(widget.config) ? { ...widget.config } : {}
    if (isLegacyWhite(config.background)) {
      config.background = 'transparent'
      config.borderColor = 'transparent'
      config.borderWidth = 0
      config.borderRadius = 0
    }
    return { ...widget, config }
  }) : []

  return {
    ...layout,
    widgets,
    connections: Array.isArray(layout.connections) ? layout.connections : []
  }
}

export function normalizeScadaProjectImport(payload, fallbackName = '导入项目') {
  const content = asJson(payload)
  const layout = findLayout(content)
  const graph = layout ? null : findGraph(content)
  if (!layout && !graph) throw new Error('文件中没有可识别的组态内容（需要 widgets/connections 或 graph.cells/cells）')
  const meta = walk(content, current => isObject(current) && (current?.name || current?.title) && (current?.layout || current?.graph || current?.cells || current?.widgets)
    ? { name: current.name || current.title, description: current.description || current.desc || '' }
    : null) || {}
  return {
    layout: normalizeImportedLayout(layout || graphToLayout(graph)),
    customComponents: findCustomComponents(content) || [],
    name: String(meta.name || fallbackName || '导入项目').replace(/\.json$/i, '').trim() || '导入项目',
    description: meta.description || (layout ? '从 Monitoring JSON 导入' : '从旧 SCADA/X6 JSON 导入'),
    format: layout ? 'Monitoring JSON' : '旧 SCADA/X6 JSON'
  }
}
