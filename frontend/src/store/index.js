import { defineStore } from 'pinia'
import { ref, computed, watch } from 'vue'
import api from '../api/index.js'

const STORAGE_KEY = 'monitor-widget-layout'
const CUSTOM_COMPONENT_STORAGE_KEY = 'monitor-custom-components'

// widget 类型定义 (含默认尺寸)
export const WIDGET_TYPES = {
  trendChart:     { type: 'trendChart',     label: '趋势曲线',   icon: '📈', defaultW: 520, defaultH: 260 },
  trendChartLite: { type: 'trendChartLite', label: '简洁趋势曲线', icon: '📉', defaultW: 460, defaultH: 240 },
  digitalDisplay: { type: 'digitalDisplay', label: '数值显示',   icon: '🔢', defaultW: 260, defaultH: 160 },
  valueColumn:    { type: 'valueColumn',    label: '液柱显示',   icon: '柱', defaultW: 140, defaultH: 300 },
  gauge:          { type: 'gauge',          label: '仪表盘',     icon: '⭕', defaultW: 260, defaultH: 260 },
  button:         { type: 'button',         label: '控制按钮',   icon: '🔘', defaultW: 140, defaultH: 80 },
  winccToggleButton: { type: 'winccToggleButton', label: '状态切换按钮', icon: '切', defaultW: 180, defaultH: 42 },
  stepperControl: { type: 'stepperControl', label: '加减调节',   icon: '±', defaultW: 120, defaultH: 40 },
  alarmList:      { type: 'alarmList',      label: '报警列表',   icon: '📋', defaultW: 520, defaultH: 260 },
  indicator:      { type: 'indicator',      label: '状态指示',   icon: '💡', defaultW: 120, defaultH: 80 },
  statusCircle:   { type: 'statusCircle',   label: '状态圆点',   icon: '●', defaultW: 42, defaultH: 42 },
  label:          { type: 'label',          label: '文本标签',   icon: '📝', defaultW: 240, defaultH: 60 },
  frameBox:       { type: 'frameBox',       label: '边框容器',   icon: '▣', defaultW: 320, defaultH: 180 },
  scadaSvg:       { type: 'scadaSvg',       label: '导入 SVG/图片', icon: '图', defaultW: 180, defaultH: 140 },
  customShape:    { type: 'customShape',    label: '自定义组件', icon: '自', defaultW: 180, defaultH: 120 },
  processPump:    { type: 'processPump',    label: '水泵/气泵',  icon: '泵', defaultW: 112, defaultH: 96 },
  processPool:    { type: 'processPool',    label: '水池',       icon: '池', defaultW: 380, defaultH: 156 },
  processDam:     { type: 'processDam',     label: '尾矿库',     icon: '库', defaultW: 540, defaultH: 140 },
  processTank:    { type: 'processTank',    label: '水罐',       icon: '罐', defaultW: 92, defaultH: 136 },
  processMixer:   { type: 'processMixer',   label: '浓密机',     icon: '机', defaultW: 280, defaultH: 210 },
  processDosingMachine: { type: 'processDosingMachine', label: '加药机', icon: '药', defaultW: 96, defaultH: 104 },
  processJunction: { type: 'processJunction', label: '连接节点', icon: '点', defaultW: 12, defaultH: 12 },
  processValve:   { type: 'processValve',   label: '横向阀门',   icon: '阀', defaultW: 82, defaultH: 50 },
  processValveVertical: { type: 'processValveVertical', label: '立式阀门', icon: '阀', defaultW: 58, defaultH: 88 },
  processPipeEnd: { type: 'processPipeEnd', label: '管道端口',   icon: '口', defaultW: 82, defaultH: 34 },
  processValueTag:{ type: 'processValueTag',label: '数据标牌',   icon: '数', defaultW: 86, defaultH: 28 },
  processCalcinerFurnace: { type: 'processCalcinerFurnace', label: '焙烧炉', icon: '炉', defaultW: 132, defaultH: 238 },
  processCycloneSeparator: { type: 'processCycloneSeparator', label: '旋风分离器', icon: '旋', defaultW: 92, defaultH: 142 },
  processPreheaterTower: { type: 'processPreheaterTower', label: '预热/冷却器', icon: '换', defaultW: 84, defaultH: 184 },
  processBagFilter: { type: 'processBagFilter', label: '布袋除尘器', icon: '尘', defaultW: 246, defaultH: 132 },
  processFanBlower: { type: 'processFanBlower', label: '引风机', icon: '风', defaultW: 122, defaultH: 104 },
  processWinccFan: { type: 'processWinccFan', label: '风机', icon: '风', defaultW: 96, defaultH: 96 },
  processBurner: { type: 'processBurner', label: '燃烧器', icon: '燃', defaultW: 126, defaultH: 70 },
  processAluminaSilo: { type: 'processAluminaSilo', label: '氧化铝料仓', icon: '仓', defaultW: 106, defaultH: 156 },
  processScrewConveyor: { type: 'processScrewConveyor', label: '螺旋输送机', icon: '螺', defaultW: 260, defaultH: 62 },
  processBucketElevator: { type: 'processBucketElevator', label: '斗式提升机', icon: '提', defaultW: 82, defaultH: 214 },
  processGasValveSkid: { type: 'processGasValveSkid', label: '燃气阀组', icon: '气', defaultW: 184, defaultH: 94 },
  processStatusMatrix: { type: 'processStatusMatrix', label: '状态灯盘', icon: '灯', defaultW: 178, defaultH: 112 },
  processHeatExchanger: { type: 'processHeatExchanger', label: '换热器', icon: '换', defaultW: 86, defaultH: 178 },
  processKilnTube: { type: 'processKilnTube', label: '炉筒子', icon: '筒', defaultW: 320, defaultH: 86 },
  processStack: { type: 'processStack', label: '烟囱/排气筒', icon: '烟', defaultW: 64, defaultH: 240 },
  processRotaryAirlock: { type: 'processRotaryAirlock', label: '星型下料阀', icon: '星', defaultW: 74, defaultH: 74 },
  processDamper: { type: 'processDamper', label: '风门挡板', icon: '挡', defaultW: 96, defaultH: 58 },
  processSprayLance: { type: 'processSprayLance', label: '喷枪', icon: '喷', defaultW: 138, defaultH: 52 },
  processMaterialChute: { type: 'processMaterialChute', label: '斜溜槽', icon: '槽', defaultW: 120, defaultH: 86 },
  processDoubleFlapValve: { type: 'processDoubleFlapValve', label: '双层翻板阀', icon: '板', defaultW: 70, defaultH: 100 },
}

const DEFAULT_WIDGETS = []

function defaultConfigForType(type) {
  const def = WIDGET_TYPES[type]
  const title = def?.label || type
  const base = { title, titleFontSize: 12, hideName: false, rotation: 0, rotationStep: 90 }
  switch (type) {
    case 'trendChart':
    case 'trendChartLite':
      return { ...base, varIds: [], yMin: null, yMax: null, timeWindow: 60 }
    case 'digitalDisplay':
      return { ...base, varId: null, min: 0, max: 100, unit: '', decimals: 1 }
    case 'valueColumn':
      return {
        ...base,
        varId: null,
        min: 0,
        max: 100,
        unit: '',
        decimals: 1,
        valuePosition: 'right',
        barColor: '#ec4899',
        trackColor: '#111827',
        scaleColor: '#98a2b3',
        textColor: '#f8fafc'
      }
    case 'gauge':
      return { ...base, varId: null, min: 0, max: 100, unit: '', decimals: 1 }
    case 'button':
      return { ...base, varId: null, buttonText: title, writeValue: 1 }
    case 'winccToggleButton':
      return { ...base, varId: null, buttonText: title, activeValue: 1, inactiveValue: 0, activeColor: '#18c93a', inactiveTopColor: '#ffffff', inactiveBottomColor: '#e5e7eb', exclusiveEnabled: false, exclusivePeerIds: [] }
    case 'stepperControl':
      return { ...base, varId: null, defaultValue: 0, min: 0, max: 100, step: 1, decimals: 0, unit: '', minusText: '-', plusText: '+' }
    case 'indicator':
      return { ...base, varId: null, threshold: 50 }
    case 'statusCircle':
      return { ...base, title: '状态圆点', hideName: true, varId: null, threshold: 0 }
    case 'label':
      return {
        ...base,
        text: title,
        fontSize: 14,
        color: '#344054',
        bold: false,
        italic: false,
        underline: false,
        align: 'center',
        verticalAlign: 'center',
        background: 'transparent',
        borderColor: 'transparent',
        borderWidth: 0,
        borderRadius: 0
      }
    case 'frameBox':
      return {
        ...base,
        title: '边框容器',
        hideName: false,
        background: '#ffffff',
        backgroundOpacity: 0,
        borderColor: '#4f6fb8',
        borderWidth: 2,
        borderRadius: 8,
        borderStyle: 'solid'
      }
    case 'scadaSvg':
      return {
        ...base,
        label: title,
        labelFontSize: 12,
        labelOffsetX: 0,
        labelOffsetY: 0,
        varId: null,
        bindVariable: false,
        statusOnColor: '#22c55e',
        statusOffColor: '#64748b',
        opacity: 100,
        background: '#ffffff',
        backgroundOpacity: 0,
        borderColor: '#4f6fb8',
        borderWidth: 0,
        borderRadius: 0,
        preserveAspectRatio: 'meet',
        asset: null
      }
    case 'customShape':
      return {
        ...base,
        label: title,
        labelFontSize: 12,
        labelOffsetX: 0,
        labelOffsetY: 0,
        varId: null,
        bindVariable: false,
        statusOnColor: '#22c55e',
        statusOffColor: '#ef4444',
        definition: {
          name: title,
          width: 180,
          height: 120,
          bindVariable: false,
          statusOnColor: '#22c55e',
          statusOffColor: '#ef4444',
          shapes: [
            { id: 'body', kind: 'rect', x: 20, y: 20, w: 140, h: 80, rotate: 0, fill: '#dbe4ea', stroke: '#263544', strokeWidth: 2, radius: 4, bindStatus: true }
          ],
          ports: [
            { id: 'left', side: 'left', x: 0, y: 50 },
            { id: 'right', side: 'right', x: 100, y: 50 }
          ]
        }
      }
    case 'processValueTag':
      return { ...base, label: '', varId: null, unit: '', decimals: 1, titleFontSize: 11, valueFontSize: 16 }
    case 'processPump':
      return {
        ...base,
        label: title,
        labelFontSize: 12,
        labelOffsetX: 0,
        labelOffsetY: 0,
        varId: null,
        flipX: false,
        flipY: false
      }
    case 'processDosingMachine':
      return { ...base, label: title, labelFontSize: 12, labelOffsetX: 0, labelOffsetY: 0, varId: null }
    case 'processJunction':
      return { ...base, label: '', hideNode: false }
    case 'processValve':
    case 'processValveVertical':
      return { ...base, label: title, labelFontSize: 12, labelOffsetX: 0, labelOffsetY: 0, varId: null, normallyOpen: false }
    case 'processPool':
      return { ...base, label: title, labelFontSize: 12, labelOffsetX: 0, labelOffsetY: 0, fill: 62 }
    case 'processDam':
      return { ...base, label: title, labelFontSize: 12, labelOffsetX: 0, labelOffsetY: 0 }
    case 'processTank':
      return { ...base, label: title, labelFontSize: 12, labelOffsetX: 0, labelOffsetY: 0, fill: 72 }
    case 'processMixer':
      return { ...base, label: title, labelFontSize: 12, labelOffsetX: 0, labelOffsetY: 0, fill: 70 }
    case 'processPipeEnd':
      return { ...base, label: title, labelFontSize: 12, labelOffsetX: 0, labelOffsetY: 0, direction: 'right' }
    case 'processAluminaSilo':
      return { ...base, label: title, labelFontSize: 12, labelOffsetX: 0, labelOffsetY: 0, varId: null, fill: 68 }
    case 'processCalcinerFurnace':
    case 'processCycloneSeparator':
    case 'processPreheaterTower':
    case 'processBagFilter':
    case 'processFanBlower':
    case 'processWinccFan':
    case 'processBurner':
    case 'processScrewConveyor':
    case 'processBucketElevator':
    case 'processGasValveSkid':
    case 'processHeatExchanger':
    case 'processKilnTube':
    case 'processStack':
    case 'processRotaryAirlock':
    case 'processDamper':
    case 'processSprayLance':
    case 'processMaterialChute':
    case 'processDoubleFlapValve':
      return { ...base, label: title, labelFontSize: 12, labelOffsetX: 0, labelOffsetY: 0, varId: null }
    case 'processStatusMatrix':
      return { ...base, label: title, labelFontSize: 12, labelOffsetX: 0, labelOffsetY: 0 }
    default:
      return base
  }
}

function normalizeLayoutPayload(payload) {
  if (Array.isArray(payload)) {
    return { widgets: payload, connections: [] }
  }
  if (payload && typeof payload === 'object') {
    return {
      widgets: Array.isArray(payload.widgets) ? payload.widgets : [],
      connections: Array.isArray(payload.connections) ? payload.connections : []
    }
  }
  return { widgets: [], connections: [] }
}

function clone(value) {
  return JSON.parse(JSON.stringify(value))
}

function finiteNumber(value, fallback = 0) {
  const number = Number(value)
  return Number.isFinite(number) ? number : fallback
}

function normalizeArrowSize(value) {
  return Math.max(4, Math.min(48, Math.round(finiteNumber(value, 10))))
}

function clampPercent(value, fallback = 50) {
  return Math.max(0, Math.min(100, finiteNumber(value, fallback)))
}

function nearestPortSide(x, y) {
  const distances = [
    { side: 'top', distance: Math.abs(y) },
    { side: 'right', distance: Math.abs(100 - x) },
    { side: 'bottom', distance: Math.abs(100 - y) },
    { side: 'left', distance: Math.abs(x) }
  ]
  distances.sort((a, b) => a.distance - b.distance)
  return distances[0].side
}

function legacyPortPosition(port) {
  const offset = clampPercent(port?.offset, 50)
  if (port?.side === 'top') return { x: offset, y: 0 }
  if (port?.side === 'bottom') return { x: offset, y: 100 }
  if (port?.side === 'left') return { x: 0, y: offset }
  return { x: 100, y: offset }
}

function normalizeCustomPort(port, index = 0) {
  const legacy = legacyPortPosition(port)
  const xSource = port?.xPercent ?? port?.x
  const ySource = port?.yPercent ?? port?.y
  const x = clampPercent(xSource, legacy.x)
  const y = clampPercent(ySource, legacy.y)
  const side = ['top', 'right', 'bottom', 'left'].includes(port?.side)
    ? port.side
    : nearestPortSide(x, y)
  return {
    id: String(port?.id || `port-${index + 1}`),
    side,
    x,
    y,
    offset: side === 'top' || side === 'bottom' ? x : y
  }
}

function connectionPortsFromDefinition(definition) {
  const ports = Array.isArray(definition?.ports) ? definition.ports : []
  return ports.map((port, index) => {
    const normalized = normalizeCustomPort(port, index)
    return {
      id: normalized.id,
      side: normalized.side,
      x: normalized.x / 100,
      y: normalized.y / 100
    }
  })
}

function rotatedBoundsForShape(shape) {
  const x = finiteNumber(shape.x, 0)
  const y = finiteNumber(shape.y, 0)
  const w = Math.max(0, finiteNumber(shape.w, 0))
  const h = Math.max(0, finiteNumber(shape.h, 0))
  const pad = Math.max(0, finiteNumber(shape.strokeWidth, 0)) / 2
  const minX = x - pad
  const minY = y - pad
  const maxX = x + w + pad
  const maxY = y + h + pad
  const rotate = finiteNumber(shape.rotate, 0) * Math.PI / 180
  if (!rotate) return { minX, minY, maxX, maxY }

  const cx = x + w / 2
  const cy = y + h / 2
  const cos = Math.cos(rotate)
  const sin = Math.sin(rotate)
  const points = [
    { x: minX, y: minY },
    { x: maxX, y: minY },
    { x: maxX, y: maxY },
    { x: minX, y: maxY }
  ].map(point => {
    const dx = point.x - cx
    const dy = point.y - cy
    return {
      x: cx + dx * cos - dy * sin,
      y: cy + dx * sin + dy * cos
    }
  })
  return {
    minX: Math.min(...points.map(point => point.x)),
    minY: Math.min(...points.map(point => point.y)),
    maxX: Math.max(...points.map(point => point.x)),
    maxY: Math.max(...points.map(point => point.y))
  }
}

function fitCustomComponentToContent(component) {
  if (!component || !Array.isArray(component.shapes) || component.shapes.length === 0) return component
  const bounds = component.shapes
    .map(rotatedBoundsForShape)
    .reduce((acc, item) => ({
      minX: Math.min(acc.minX, item.minX),
      minY: Math.min(acc.minY, item.minY),
      maxX: Math.max(acc.maxX, item.maxX),
      maxY: Math.max(acc.maxY, item.maxY)
    }), { minX: Infinity, minY: Infinity, maxX: -Infinity, maxY: -Infinity })

  if (!Number.isFinite(bounds.minX) || !Number.isFinite(bounds.minY) ||
      !Number.isFinite(bounds.maxX) || !Number.isFinite(bounds.maxY)) {
    return component
  }

  const nextWidth = Math.max(1, Math.ceil(bounds.maxX - bounds.minX))
  const nextHeight = Math.max(1, Math.ceil(bounds.maxY - bounds.minY))
  const sourceWidth = Math.max(1, finiteNumber(component.width, nextWidth))
  const sourceHeight = Math.max(1, finiteNumber(component.height, nextHeight))

  return {
    ...component,
    width: nextWidth,
    height: nextHeight,
    shapes: component.shapes.map(shape => ({
      ...shape,
      x: Math.round((finiteNumber(shape.x, 0) - bounds.minX) * 10) / 10,
      y: Math.round((finiteNumber(shape.y, 0) - bounds.minY) * 10) / 10
    })),
    ports: component.ports.map(port => {
      const oldX = sourceWidth * clampPercent(port.x, 50) / 100
      const oldY = sourceHeight * clampPercent(port.y, 50) / 100
      return {
        ...port,
        x: clampPercent((oldX - bounds.minX) / nextWidth * 100, port.x),
        y: clampPercent((oldY - bounds.minY) / nextHeight * 100, port.y)
      }
    })
  }
}

export const useMonitorStore = defineStore('monitor', () => {
  // ============ 原始数据 ============
  const variables        = ref([])
  const realtimeData     = ref({})
  const wsConnected      = ref(false)
  const alarms           = ref([])
  const systemStatus     = ref({ opcuaServer: false, opcuaClient: false, udpReceiver: false, database: false })
  const operationLogs    = ref([])
  const customComponents = ref([])
    // ============ 趋势曲线数据缓冲 (持久化, 跨页面切换) ============
  const trendBuffers = ref({})
  const TREND_MAX_POINTS = 2000
  const TREND_TIME_WINDOW = 600000
  const TREND_MIN_INTERVAL = 200
  let trendSamplerTimer = null

  function appendTrendData(varId, value, timestampMs = Date.now()) {
    if (varId == null || typeof value !== 'number' || !Number.isFinite(value)) return
    const buffers = trendBuffers.value
    if (!buffers[varId]) buffers[varId] = []
    const buf = buffers[varId]
    const last = buf[buf.length - 1]
    let time = Number(timestampMs) || Date.now()
    if (last) {
      if (time <= last.time) time = last.time + 1
      if (time - last.time < TREND_MIN_INTERVAL && last.value === value) return
    }
    buf.push({ time, value })
    if (buffers[varId].length > TREND_MAX_POINTS) buffers[varId].shift()
  }

  function sampleTrendData(varIds = null) {
    const ids = Array.isArray(varIds) ? varIds : Object.keys(realtimeData.value)
    const time = Date.now()
    ids.forEach(id => {
      const dp = realtimeData.value[id]
      if (dp && typeof dp.value === 'number') appendTrendData(dp.varId ?? id, dp.value, time)
    })
    pruneTrendData()
  }

  function startTrendSampler(intervalMs = 1000) {
    if (trendSamplerTimer) return
    sampleTrendData()
    trendSamplerTimer = setInterval(() => sampleTrendData(), intervalMs)
  }

  function stopTrendSampler() {
    if (!trendSamplerTimer) return
    clearInterval(trendSamplerTimer)
    trendSamplerTimer = null
  }

  function pruneTrendData() {
    const buffers = trendBuffers.value
    const tStart = Date.now() - TREND_TIME_WINDOW
    for (const vid in buffers) {
      const buf = buffers[vid]
      while (buf.length > 0 && buf[0].time < tStart) buf.shift()
    }
  }
    // ============ Widget 布局 (像素坐标) ============
  const widgets = ref([])
  const connections = ref([])
  const nextId  = ref(Date.now())
  const nextConnectionId = ref(Date.now())
  let layoutPersistTimer = null
  const undoStack = []
  const redoStack = []
  const canUndo = ref(false)
  const canRedo = ref(false)
  const HISTORY_LIMIT = 100
  const HISTORY_COALESCE_MS = 700
  let lastHistoryKey = null
  let lastHistoryAt = 0

  function normalizeCustomComponent(component) {
    if (!component || typeof component !== 'object') return null
    const id = String(component.id || `custom-${Date.now()}`)
    const name = String(component.name || '自定义组件').trim() || '自定义组件'
    const width = Math.max(1, finiteNumber(component.width, WIDGET_TYPES.customShape.defaultW))
    const height = Math.max(1, finiteNumber(component.height, WIDGET_TYPES.customShape.defaultH))
    const customShapeKinds = ['line', 'line-v', 'slash', 'backslash', 'rect', 'square', 'round-rect', 'capsule', 'circle', 'ellipse', 'triangle', 'right-triangle', 'trapezoid', 'parallelogram', 'diamond', 'pentagon', 'hexagon', 'octagon', 'star', 'chevron', 'cross', 'arc', 'arrow-left', 'arrow-right', 'pipe-elbow', 'pipe-tee', 'cube', 'cylinder', 'cylinder-h', 'cone', 'frustum', 'tri-prism']
    const shapes = Array.isArray(component.shapes)
      ? component.shapes.filter(shape => shape && typeof shape === 'object').map((shape, index) => ({
          id: String(shape.id || `shape-${index + 1}`),
          kind: customShapeKinds.includes(shape.kind) ? shape.kind : 'rect',
          x: Number(shape.x) || 0,
          y: Number(shape.y) || 0,
          w: Math.max(0, finiteNumber(shape.w, 40)),
          h: Math.max(0, finiteNumber(shape.h, 40)),
          rotate: finiteNumber(shape.rotate, 0),
          fill: ['arrow-right', 'arrow-left'].includes(shape.kind) && (!shape.fill || shape.fill === 'transparent') ? '#263544' : (shape.fill || '#dbe4ea'),
          stroke: shape.stroke || '#263544',
          strokeWidth: Math.max(0, Number(shape.strokeWidth) || 0),
          radius: Math.max(0, Number(shape.radius) || 0),
          bindStatus: shape.bindStatus !== false
        }))
      : []
    const ports = Array.isArray(component.ports)
      ? component.ports.filter(port => port && typeof port === 'object').map(normalizeCustomPort)
      : []
    return {
      id,
      name,
      description: component.description || '',
      width,
      height,
      bindVariable: Boolean(component.bindVariable),
      statusOnColor: component.statusOnColor || '#22c55e',
      statusOffColor: component.statusOffColor || '#ef4444',
      shapes,
      ports
    }
  }

  function customComponentToWidgetConfig(component) {
    const normalized = normalizeCustomComponent(component)
    const definition = normalized || defaultConfigForType('customShape').definition
    return {
      ...defaultConfigForType('customShape'),
      title: definition.name,
      label: definition.name,
      varId: null,
      bindVariable: Boolean(definition.bindVariable),
      statusOnColor: definition.statusOnColor,
      statusOffColor: definition.statusOffColor,
      definition,
      connectionPorts: connectionPortsFromDefinition(definition)
    }
  }

  function loadCustomComponents() {
    try {
      const saved = localStorage.getItem(CUSTOM_COMPONENT_STORAGE_KEY)
      const parsed = saved ? JSON.parse(saved) : []
      const localComponents = Array.isArray(parsed)
        ? parsed.map(normalizeCustomComponent).filter(Boolean)
        : []
      customComponents.value = localComponents
    } catch {
      customComponents.value = []
    }
    // 后端作为跨端共享源：异步拉取并合并（后端为空时保留本地）
    refreshCustomComponentsFromServer()
  }

  function refreshCustomComponentsFromServer() {
    api.getCustomComponents()
      .then(r => {
        const list = Array.isArray(r?.data) ? r.data : []
        if (list.length) {
          customComponents.value = list.map(normalizeCustomComponent).filter(Boolean)
          localStorage.setItem(CUSTOM_COMPONENT_STORAGE_KEY, JSON.stringify(customComponents.value))
        }
      })
      .catch(() => {})
  }

  function persistCustomComponents() {
    localStorage.setItem(CUSTOM_COMPONENT_STORAGE_KEY, JSON.stringify(customComponents.value))
    // 同步到后端组件库（失败不影响本地使用）
    api.saveCustomComponents(customComponents.value).catch(() => {})
  }

  // 从后端导入组件（合并）：返回 Promise<number> 导入数量
  async function importCustomComponentsFromServer(payload) {
    const r = await api.importCustomComponents(payload)
    const list = Array.isArray(r?.data) ? r.data : []
    if (list.length) {
      customComponents.value = list.map(normalizeCustomComponent).filter(Boolean)
      localStorage.setItem(CUSTOM_COMPONENT_STORAGE_KEY, JSON.stringify(customComponents.value))
    }
    return list.length
  }

  function saveCustomComponent(component) {
    const normalized = normalizeCustomComponent(component)
    if (!normalized) return null
    const idx = customComponents.value.findIndex(item => item.id === normalized.id)
    if (idx >= 0) customComponents.value[idx] = normalized
    else customComponents.value.push(normalized)
    persistCustomComponents()
    return normalized
  }

  function removeCustomComponent(id) {
    customComponents.value = customComponents.value.filter(item => item.id !== id)
    persistCustomComponents()
  }

  function normalizeWidget(widget) {
    if (!widget || typeof widget !== 'object') return null
    const type = String(widget.type || '')
    if (!type) return null
    const def = WIDGET_TYPES[type]
    const rawConfig = widget.config && typeof widget.config === 'object' ? widget.config : {}
    const config = { ...defaultConfigForType(type), ...rawConfig }
    if (type === 'customShape') {
      const definition = normalizeCustomComponent(config.definition || defaultConfigForType('customShape').definition)
      if (definition) {
        config.definition = definition
        config.connectionPorts = connectionPortsFromDefinition(definition)
      }
    }
    return {
      ...widget,
      id: String(widget.id || genId()),
      type,
      x: finiteNumber(widget.x, 40),
      y: finiteNumber(widget.y, 40),
      w: Math.max(1, finiteNumber(widget.w, def?.defaultW ?? 240)),
      h: Math.max(1, finiteNumber(widget.h, def?.defaultH ?? 120)),
      config
    }
  }

  function loadLayout() {
    loadCustomComponents()
    try {
      const saved = localStorage.getItem(STORAGE_KEY)
      if (saved) {
        const layout = normalizeLayoutPayload(JSON.parse(saved))
        widgets.value = layout.widgets.map(normalizeWidget).filter(Boolean)
        connections.value = layout.connections.map(normalizeConnection).filter(Boolean)
        widgets.value.forEach(w => {
          const n = parseInt(w.id.replace('w-', ''))
          if (n >= nextId.value) nextId.value = n + 1
        })
        connections.value.forEach(c => {
          const n = parseInt(String(c.id).replace('c-', ''))
          if (n >= nextConnectionId.value) nextConnectionId.value = n + 1
        })
      } else {
        widgets.value = clone(DEFAULT_WIDGETS).map(normalizeWidget).filter(Boolean)
        connections.value = []
      }
    } catch {
      widgets.value = clone(DEFAULT_WIDGETS).map(normalizeWidget).filter(Boolean)
      connections.value = []
    }
    undoStack.length = 0
    redoStack.length = 0
    lastHistoryKey = null
    lastHistoryAt = 0
    syncHistoryState()
  }

  const layoutSnapshot = computed(() => ({
    widgets: clone(widgets.value),
    connections: clone(connections.value)
  }))

  function persistLayout() {
    localStorage.setItem(STORAGE_KEY, JSON.stringify(layoutSnapshot.value))
  }

  function scheduleLayoutPersist() {
    if (layoutPersistTimer) clearTimeout(layoutPersistTimer)
    layoutPersistTimer = setTimeout(() => {
      layoutPersistTimer = null
      persistLayout()
    }, 160)
  }

  watch([widgets, connections], () => {
    scheduleLayoutPersist()
  }, { deep: true })

  function genId() { return 'w-' + (nextId.value++) }
  function genConnectionId() { return 'c-' + (nextConnectionId.value++) }

  function captureLayout() {
    return {
      widgets: clone(widgets.value),
      connections: clone(connections.value)
    }
  }

  function syncHistoryState() {
    canUndo.value = undoStack.length > 0
    canRedo.value = redoStack.length > 0
  }

  function recordHistory(key, coalesce = false) {
    const now = Date.now()
    const mergeWithPrevious = coalesce && key === lastHistoryKey && now - lastHistoryAt <= HISTORY_COALESCE_MS
    if (!mergeWithPrevious) {
      undoStack.push(captureLayout())
      if (undoStack.length > HISTORY_LIMIT) undoStack.shift()
      redoStack.length = 0
    }
    lastHistoryKey = key
    lastHistoryAt = now
    syncHistoryState()
  }

  function restoreHistorySnapshot(snapshot) {
    widgets.value = clone(snapshot.widgets || []).map(normalizeWidget).filter(Boolean)
    connections.value = clone(snapshot.connections || []).map(normalizeConnection).filter(Boolean)
    lastHistoryKey = null
    lastHistoryAt = 0
  }

  function undoLayout() {
    const snapshot = undoStack.pop()
    if (!snapshot) return false
    redoStack.push(captureLayout())
    restoreHistorySnapshot(snapshot)
    syncHistoryState()
    return true
  }

  function redoLayout() {
    const snapshot = redoStack.pop()
    if (!snapshot) return false
    undoStack.push(captureLayout())
    restoreHistorySnapshot(snapshot)
    syncHistoryState()
    return true
  }

  function normalizeConnection(connection) {
    if (!connection) return null
    const hasEndpointWidgets = connection.from && connection.to
    const points = Array.isArray(connection.points)
      ? connection.points
          .map(p => ({ x: Number(p.x), y: Number(p.y) }))
          .filter(p => Number.isFinite(p.x) && Number.isFinite(p.y))
      : null
    if (!hasEndpointWidgets && points && points.length >= 2) {
      return {
        id: connection.id || genConnectionId(),
        from: connection.from || null,
        to: connection.to || null,
        points,
        color: connection.color || '#176e45',
        width: Number(connection.width) || 1,
        startArrow: Boolean(connection.startArrow),
        endArrow: Boolean(connection.endArrow),
        arrowSize: normalizeArrowSize(connection.arrowSize),
        kind: connection.kind || 'pipe',
        routeMode: 'points',
        bendX: null,
        bendY: null
      }
    }
    if (!hasEndpointWidgets) return null
    const normalizeAnchor = (anchor) => {
      if (!anchor || typeof anchor !== 'object') return null
      const x = Number(anchor.x)
      const y = Number(anchor.y)
      const side = ['top', 'right', 'bottom', 'left'].includes(anchor.side) ? anchor.side : null
      if (!Number.isFinite(x) || !Number.isFinite(y) || !side) return null
      return {
        x: Math.max(0, Math.min(1, x)),
        y: Math.max(0, Math.min(1, y)),
        side,
        free: Boolean(anchor.free)
      }
    }
    const waypoints = Array.isArray(connection.waypoints)
      ? connection.waypoints
          .map(p => ({ x: Number(p.x), y: Number(p.y) }))
          .filter(p => Number.isFinite(p.x) && Number.isFinite(p.y))
      : null
    const pointWaypoints = (!waypoints || !waypoints.length) && points && points.length > 2
      ? points.slice(1, -1)
      : null
    return {
      id: connection.id || genConnectionId(),
      from: connection.from,
      to: connection.to,
      fromPort: connection.fromPort || null,
      toPort: connection.toPort || null,
      fromAnchor: normalizeAnchor(connection.fromAnchor),
      toAnchor: normalizeAnchor(connection.toAnchor),
      waypoints: waypoints && waypoints.length ? waypoints : (pointWaypoints && pointWaypoints.length ? pointWaypoints : null),
      points: null,
      color: connection.color || '#178447',
      width: Number(connection.width) || 1,
      startArrow: Boolean(connection.startArrow),
      endArrow: Boolean(connection.endArrow),
      arrowSize: normalizeArrowSize(connection.arrowSize),
      kind: connection.kind || 'pipe',
      manualRoute: Boolean(connection.manualRoute),
      routeMode: connection.routeMode || null,
      bendX: Number.isFinite(Number(connection.bendX)) ? Number(connection.bendX) : null,
      bendY: Number.isFinite(Number(connection.bendY)) ? Number(connection.bendY) : null
    }
  }

  function addWidget(type, x = 40, y = 40, options = {}) {
    const def = WIDGET_TYPES[type]
    if (!def) return null
    recordHistory('add-widget')
    const customConfig = options.customComponent
      ? customComponentToWidgetConfig(options.customComponent)
      : (options.config || {})
    const w = {
      id: genId(),
      type,
      x,
      y,
      w: Math.max(1, Number(options.w) || Number(options.customComponent?.width) || def.defaultW),
      h: Math.max(1, Number(options.h) || Number(options.customComponent?.height) || def.defaultH),
      config: { ...defaultConfigForType(type), ...customConfig }
    }
    if (type === 'frameBox') widgets.value.unshift(w)
    else widgets.value.push(w)
    return w
  }

  function duplicateWidget(source, x = null, y = null) {
    if (!source || !WIDGET_TYPES[source.type]) return null
    recordHistory('paste-widget')
    const copy = normalizeWidget({
      ...clone(source),
      id: genId(),
      x: Number.isFinite(Number(x)) ? Number(x) : (Number(source.x) || 0) + 24,
      y: Number.isFinite(Number(y)) ? Number(y) : (Number(source.y) || 0) + 24,
      config: clone(source.config || {})
    })
    if (copy.type === 'frameBox') widgets.value.unshift(copy)
    else widgets.value.push(copy)
    return copy
  }

  function offsetConnectionGeometry(connection, dx, dy) {
    const next = clone(connection)
    const movePoint = point => ({
      x: finiteNumber(point?.x, 0) + dx,
      y: finiteNumber(point?.y, 0) + dy
    })
    if (Array.isArray(next.points)) next.points = next.points.map(movePoint)
    if (Array.isArray(next.waypoints)) next.waypoints = next.waypoints.map(movePoint)
    if (Number.isFinite(Number(next.bendX))) next.bendX = Number(next.bendX) + dx
    if (Number.isFinite(Number(next.bendY))) next.bendY = Number(next.bendY) + dy
    return next
  }

  function duplicateWidgetsWithConnections(sourceWidgets, sourceConnections = null, offset = {}) {
    const sources = Array.isArray(sourceWidgets)
      ? sourceWidgets.filter(widget => widget && WIDGET_TYPES[widget.type])
      : []
    if (!sources.length) return { widgets: [], connections: [] }

    const dx = finiteNumber(offset.x, 24)
    const dy = finiteNumber(offset.y, 24)
    const sourceIds = new Set(sources.map(widget => String(widget.id)))
    const connectionsToCopy = Array.isArray(sourceConnections)
      ? sourceConnections
      : connections.value.filter(connection => sourceIds.has(String(connection.from)) && sourceIds.has(String(connection.to)))

    recordHistory('paste-selection')
    const idMap = new Map()
    const nextWidgets = sources
      .map(source => {
        const nextId = genId()
        idMap.set(String(source.id), nextId)
        return normalizeWidget({
          ...clone(source),
          id: nextId,
          x: finiteNumber(source.x, 0) + dx,
          y: finiteNumber(source.y, 0) + dy,
          config: clone(source.config || {})
        })
      })
      .filter(Boolean)

    const frameWidgets = nextWidgets.filter(widget => widget.type === 'frameBox')
    const normalWidgets = nextWidgets.filter(widget => widget.type !== 'frameBox')
    widgets.value.push(...frameWidgets, ...normalWidgets)

    const nextConnections = connectionsToCopy
      .filter(connection => idMap.has(String(connection.from)) && idMap.has(String(connection.to)))
      .map(connection => normalizeConnection({
        ...offsetConnectionGeometry(connection, dx, dy),
        id: genConnectionId(),
        from: idMap.get(String(connection.from)),
        to: idMap.get(String(connection.to))
      }))
      .filter(Boolean)
    connections.value.push(...nextConnections)

    return { widgets: nextWidgets, connections: nextConnections }
  }

  function updateWidget(id, data, historyKey = `update-widget:${id}`) {
    const idx = widgets.value.findIndex(w => w.id === id)
    if (idx !== -1) {
      recordHistory(historyKey, true)
      const next = { ...data }
      if (Object.prototype.hasOwnProperty.call(next, 'config')) {
        next.config = { ...defaultConfigForType(widgets.value[idx].type), ...(next.config || {}) }
      }
      Object.assign(widgets.value[idx], next)
    }
  }

  function removeWidget(id) {
    if (!widgets.value.some(w => w.id === id)) return
    recordHistory('remove-widget')
    widgets.value = widgets.value.filter(w => w.id !== id)
    connections.value = connections.value.filter(c => !c.from || !c.to || (c.from !== id && c.to !== id))
  }

  function removeWidgets(ids = []) {
    const removeSet = new Set((Array.isArray(ids) ? ids : []).map(id => String(id)))
    if (!removeSet.size || !widgets.value.some(widget => removeSet.has(String(widget.id)))) return []
    recordHistory('remove-widgets')
    const removed = widgets.value.filter(widget => removeSet.has(String(widget.id)))
    widgets.value = widgets.value.filter(widget => !removeSet.has(String(widget.id)))
    connections.value = connections.value.filter(connection =>
      !(connection.from && removeSet.has(String(connection.from))) &&
      !(connection.to && removeSet.has(String(connection.to)))
    )
    return removed
  }

  function moveWidget(id, x, y)  { updateWidget(id, { x, y }, `move-widget:${id}`) }
  function resizeWidget(id, w, h) { updateWidget(id, { w, h }, `resize-widget:${id}`) }

  function addConnection(from, to, options = {}) {
    if (!from || !to) return null
    if (from === to && !options.allowSelf) return null
    const connectionOptions = { ...options }
    const skipHistory = Boolean(connectionOptions.skipHistory)
    delete connectionOptions.skipHistory
    const sameAnchor = (a, b) => {
      if (!a && !b) return true
      if (!a || !b) return false
      return a.side === b.side &&
        Boolean(a.free) === Boolean(b.free) &&
        Math.abs(Number(a.x) - Number(b.x)) < 0.001 &&
        Math.abs(Number(a.y) - Number(b.y)) < 0.001
    }
    const exists = connections.value.find(c =>
      c.from === from &&
      c.to === to &&
      (c.fromPort || null) === (connectionOptions.fromPort || null) &&
      (c.toPort || null) === (connectionOptions.toPort || null) &&
      sameAnchor(c.fromAnchor, connectionOptions.fromAnchor) &&
      sameAnchor(c.toAnchor, connectionOptions.toAnchor)
    )
    if (exists) return exists
    if (!skipHistory) recordHistory('add-connection')
    const connection = normalizeConnection({ id: genConnectionId(), from, to, ...connectionOptions })
    if (!connection) return null
    connections.value.push(connection)
    return connection
  }

  function splitConnectionWithJunction(connectionId, point, beforeWaypoints = [], afterWaypoints = [], options = {}) {
    const original = connections.value.find(connection => connection.id === connectionId)
    const px = Number(point?.x)
    const py = Number(point?.y)
    if (!original?.from || !original?.to || !Number.isFinite(px) || !Number.isFinite(py)) return null

    recordHistory('split-connection')
    const size = WIDGET_TYPES.processJunction.defaultW
    const junction = normalizeWidget({
      id: genId(),
      type: 'processJunction',
      x: Math.round(px - size / 2),
      y: Math.round(py - size / 2),
      w: size,
      h: size,
      config: defaultConfigForType('processJunction')
    })
    widgets.value.push(junction)
    connections.value = connections.value.filter(connection => connection.id !== connectionId)

    const cleanWaypoints = input => (Array.isArray(input) ? input : [])
      .map(item => ({ x: Number(item.x), y: Number(item.y) }))
      .filter(item => Number.isFinite(item.x) && Number.isFinite(item.y))
    const before = cleanWaypoints(beforeWaypoints)
    const after = cleanWaypoints(afterWaypoints)
    const style = { color: original.color, width: original.width, arrowSize: original.arrowSize, kind: original.kind }
    const incoming = normalizeConnection({
      id: genConnectionId(),
      from: original.from,
      to: junction.id,
      fromPort: original.fromPort,
      fromAnchor: original.fromAnchor,
      toAnchor: options.incomingJunctionAnchor || null,
      waypoints: before.length ? before : null,
      manualRoute: before.length > 0,
      ...style
    })
    const outgoing = normalizeConnection({
      id: genConnectionId(),
      from: junction.id,
      to: original.to,
      fromAnchor: options.outgoingJunctionAnchor || null,
      toPort: original.toPort,
      toAnchor: original.toAnchor,
      waypoints: after.length ? after : null,
      manualRoute: after.length > 0,
      ...style
    })
    if (incoming) connections.value.push(incoming)
    if (outgoing) connections.value.push(outgoing)
    return { junction, incoming, outgoing, original }
  }

  function removeConnection(id) {
    if (!connections.value.some(c => c.id === id)) return
    recordHistory('remove-connection')
    connections.value = connections.value.filter(c => c.id !== id)
  }

  function updateConnection(id, data) {
    const idx = connections.value.findIndex(c => c.id === id)
    if (idx === -1) return
    recordHistory(`update-connection:${id}`, true)
    connections.value[idx] = normalizeConnection({ ...connections.value[idx], ...data })
  }

  function resetLayout() {
    recordHistory('reset-layout')
    widgets.value = clone(DEFAULT_WIDGETS).map(normalizeWidget).filter(Boolean)
    connections.value = []
  }

  function replaceLayout(layout) {
    recordHistory('replace-layout')
    const normalized = normalizeLayoutPayload(layout)
    widgets.value = clone(normalized.widgets).map(normalizeWidget).filter(Boolean)
    connections.value = clone(normalized.connections).map(normalizeConnection).filter(Boolean)
  }

    // ============ 计算属性 ============
  const onlineCount = computed(() => {
    let c = 0
    for (const k in realtimeData.value) {
      if (realtimeData.value[k]?.quality === 'GOOD') c++
    }
    return c
  })

    // ============ 数据加载 ============
  function isExpectedRequestError(error) {
    const message = String(error?.message || '').toLowerCase()
    return Boolean(
      error?.code === 'ERR_CANCELED' ||
      error?.code === 'ECONNABORTED' ||
      message.includes('aborted') ||
      message.includes('canceled') ||
      message.includes('cancelled')
    )
  }

  async function loadVariables() {
    try { variables.value = (await api.getVariables()).data }
    catch (e) { if (!isExpectedRequestError(e)) console.warn('loadVariables failed:', e?.message || e) }
  }

  async function loadRealtime() {
    try {
      const res = await api.getRealtime()
      const d = {}
      res.data.forEach(dp => { d[dp.varId] = dp })
      realtimeData.value = d
      sampleTrendData(Object.keys(d))
    } catch (e) { if (!isExpectedRequestError(e)) console.warn('loadRealtime failed:', e?.message || e) }
  }

  async function loadAlarms() {
    try { alarms.value = (await api.getAlarms()).data }
    catch (e) { if (!isExpectedRequestError(e)) console.warn('loadAlarms failed:', e?.message || e) }
  }

  async function fetchStatus() {
    try { systemStatus.value = (await api.getStatus()).data }
    catch { /* ignore */ }
  }

  async function loadLogs() {
    try { operationLogs.value = (await api.getLogs()).data }
    catch { /* ignore */ }
  }

  // ============ WebSocket ============
  let ws = null

  function connectWebSocket() {
    if (ws && (ws.readyState === WebSocket.OPEN || ws.readyState === WebSocket.CONNECTING)) return
    const token = localStorage.getItem('auth_token')
    if (!token) return
    const protocol = location.protocol === 'https:' ? 'wss:' : 'ws:'
    const url = `${protocol}//${location.host}/ws/realtime?token=${encodeURIComponent(token)}`
    ws = new WebSocket(url)
    ws.onopen  = () => { wsConnected.value = true }
    ws.onclose = () => {
      wsConnected.value = false
      setTimeout(() => connectWebSocket(), 3000)
    }
    ws.onmessage = (event) => {
      try {
        const data = JSON.parse(event.data)
        if (Array.isArray(data)) {
          data.forEach(dp => {
            realtimeData.value[dp.varId] = dp
            appendTrendData(dp.varId, Number(dp.value), Date.now())
          })
        } else if (data.varId) {
          realtimeData.value[data.varId] = data
          appendTrendData(data.varId, Number(data.value), Date.now())
        }
      } catch {}
    }
  }

  function disconnectWebSocket() { if (ws) { ws.close(); ws = null } }

  return {
    variables, realtimeData, wsConnected, alarms, systemStatus, operationLogs, onlineCount, customComponents,
      trendBuffers, appendTrendData, sampleTrendData, startTrendSampler, stopTrendSampler, pruneTrendData, TREND_MAX_POINTS,
    widgets, connections, layoutSnapshot, canUndo, canRedo,
    loadCustomComponents, refreshCustomComponentsFromServer, importCustomComponentsFromServer,
    saveCustomComponent, removeCustomComponent,
    loadLayout, addWidget, duplicateWidget, duplicateWidgetsWithConnections, updateWidget, removeWidget, removeWidgets, moveWidget, resizeWidget,
    addConnection, splitConnectionWithJunction, removeConnection, updateConnection, resetLayout, replaceLayout,
    undoLayout, redoLayout,
    loadVariables, loadRealtime, loadAlarms, fetchStatus, loadLogs,
    connectWebSocket, disconnectWebSocket,
  }
})
