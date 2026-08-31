<template>
  <div
    class="widget-wrapper"
    :class="{
      dragging: isDragging,
      resizing: isResizing,
      selected: !readonly && selected,
      readonly,
      'button-shell': isButtonWidget,
      'process-shell': isProcessWidget,
      'process-value-tag-shell': isProcessValueTagWidget,
      'custom-shell': isCustomShapeWidget,
      'label-shell': isLabelWidget,
      'status-circle-shell': isStatusCircleWidget,
      'frame-shell': isFrameBoxWidget,
      'scada-svg-shell': isScadaSvgWidget,
      'ports-visible': showConnectionPorts,
      'name-hidden': hideName,
      'connection-mode': connectionMode,
      'connection-source': connectionSource
    }"
    :style="wrapperStyle"
    :data-widget-id="widget.id"
    :data-zoom-refresh="zoomRefreshTick"
    @mousedown.stop="onMouseDown"
  >
    <div v-if="!isFramelessWidget && !hideName" class="ww-header" :style="{ borderTopColor: headerColor }">
      <span class="ww-title" :style="titleStyle">{{ displayTitle }}</span>
    </div>
    <div v-if="!readonly && selected" class="ww-actions ww-floating-actions" @mousedown.stop @mouseup.stop @click.stop>
      <button type="button" class="btn-icon widget-action-btn" title="编辑组件" @mousedown.stop.prevent="openConfig" @click.stop.prevent="openConfig">⚙</button>
      <button type="button" class="btn-icon widget-action-btn" title="删除组件" @mousedown.stop.prevent="remove" @click.stop.prevent="remove">✕</button>
    </div>
    <div class="ww-body" :class="{ 'ww-body-full': isFramelessWidget }">
      <WidgetFactory :widget="widget" :readonly="readonly" />
    </div>
    <div
      v-if="isFrameBoxWidget && !readonly"
      class="frame-edge-hit"
      title="拖动边框移动容器"
      @mousedown.stop="onMouseDown"
    >
      <span class="frame-edge edge-top"></span>
      <span class="frame-edge edge-right"></span>
      <span class="frame-edge edge-bottom"></span>
      <span class="frame-edge edge-left"></span>
    </div>
    <div v-if="showConnectionPorts" class="connection-ports">
      <button
        v-for="port in connectionPorts"
        :key="port.key"
        class="connection-port-anchor"
        :class="`port-${port.name}`"
        :style="port.style"
        :data-widget-id="widget.id"
        :data-port-name="port.name"
        :data-anchor-x="port.connectionAnchor?.x"
        :data-anchor-y="port.connectionAnchor?.y"
        :title="`${displayTitle} / ${port.name}`"
        @mousedown.stop.prevent="startPortConnection(port.name, $event, port.connectionAnchor)"
      ></button>
      <span
        v-for="port in connectionPorts"
        :key="`${port.key}-guide`"
        class="connection-port-guide"
        :class="`guide-${port.name}`"
        :style="port.guideStyle"
      ></span>
      <button
        v-for="port in customConnectionPorts"
        :key="port.key"
        class="connection-port-anchor custom-port-anchor"
        :class="`port-${port.name}`"
        :style="port.style"
        :data-widget-id="widget.id"
        :data-port-name="port.name"
        :data-anchor-x="port.anchor.x"
        :data-anchor-y="port.anchor.y"
        :title="port.title || '自定义连接端点'"
        @mousedown.stop.prevent="startPortConnection(port.name, $event, port.anchor)"
      ></button>
    </div>
    <div
      v-if="!readonly && selected"
      class="resize-handle br scale-handle"
      title="等比缩放"
      @mousedown.stop="(e) => startResize('se', e)"
    >
      <span class="scale-handle-icon" aria-hidden="true"></span>
    </div>
    <div v-if="!readonly && selected" class="resize-handle r" @mousedown.stop="(e) => startResize('e', e)"></div>
    <div v-if="!readonly && selected" class="resize-handle b" @mousedown.stop="(e) => startResize('s', e)"></div>

  </div>
</template>

<script setup>
import { ref, computed, inject } from 'vue'
import { useMonitorStore } from '../../store/index.js'
import WidgetFactory from './WidgetFactory.vue'
import { vectorPort } from '../../services/vectorModelService.js'

const props = defineProps({
  widget: Object,
  selected: Boolean,
  readonly: Boolean,
  connectionMode: Boolean,
  connectionSource: Boolean,
  forceShowPorts: Boolean,
  canvasOffset: {
    type: Object,
    default: () => ({ x: 0, y: 0 })
  }
})
const emit = defineEmits(['select', 'open-config', 'request-remove', 'port-connection-start', 'custom-port-connection-start'])
const store = useMonitorStore()
const canvasZoom = inject('canvasZoom', ref(1))
const zoomRefreshTick = inject('zoomRefreshTick', ref(0))
const alignWidgetDrag = inject('alignWidgetDrag', null)
const clearWidgetDragAlignment = inject('clearWidgetDragAlignment', null)
const beginWidgetTransform = inject('beginWidgetTransform', null)
const endWidgetTransform = inject('endWidgetTransform', null)

const isDragging = ref(false)
const isResizing = ref(false)
const MIN_RESIZE_SIZE = 12
const DRAG_START_THRESHOLD = 3

const typeColors = {
  trendChart: '#4f6fb8', trendChartLite: '#3b82f6', digitalDisplay: '#2f8f63', valueColumn: '#ec4899', gauge: '#a66a1f',
  button: '#c2414b', winccToggleButton: '#18c93a', stepperControl: '#64748b', alarmList: '#a66a1f', indicator: '#6f5fb8', statusCircle: '#22c55e', label: '#667085',
  frameBox: '#4f6fb8',
  customShape: '#2563eb',
  scadaSvg: '#0ea5e9',
  processPump: '#178447', processPool: '#0ea5b7', processTank: '#64748b',
  processDam: '#111827', processMixer: '#0ea5e9', processDosingMachine: '#256f9c', processJunction: '#178447', processValve: '#16a34a',
  processValveVertical: '#16a34a', processPipeEnd: '#14b8a6', processValueTag: '#a66a1f',
  processCalcinerFurnace: '#8b5e34', processCycloneSeparator: '#64748b', processPreheaterTower: '#64748b',
  processBagFilter: '#7c8b97', processFanBlower: '#15803d', processWinccFan: '#39d10f', processBurner: '#dc2626',
  processAluminaSilo: '#8a9398', processScrewConveyor: '#475569', processBucketElevator: '#475569',
  processGasValveSkid: '#d99a1a', processStatusMatrix: '#4f6fb8',
  processHeatExchanger: '#64748b', processKilnTube: '#475569', processStack: '#94a3b8',
  processRotaryAirlock: '#16a34a', processDamper: '#16a34a', processSprayLance: '#dc2626',
  processMaterialChute: '#64748b', processDoubleFlapValve: '#16a34a'
}
const headerColor = computed(() => typeColors[props.widget.type] || '#4f6fb8')

const typeLabels = {
  trendChart: '趋势曲线',
  trendChartLite: '简洁趋势曲线',
  digitalDisplay: '数值显示',
  valueColumn: '液柱显示',
  gauge: '仪表盘',
  button: '控制按钮',
  winccToggleButton: '状态切换按钮',
  stepperControl: '加减调节',
  indicator: '状态指示',
  statusCircle: '状态圆点',
  label: '文本标签',
  frameBox: '边框容器',
  customShape: '自定义组件',
  scadaSvg: 'Scada组件',
  alarmList: '报警列表',
  processPump: '水泵/气泵',
  processDosingMachine: '加药机',
  processJunction: '连接节点',
  processPool: '水池',
  processDam: '尾矿库',
  processTank: '水罐',
  processMixer: '浓密机',
  processValve: '横向阀门',
  processValveVertical: '立式阀门',
  processPipeEnd: '管道端口',
  processValueTag: '数据标牌',
  processCalcinerFurnace: '焙烧炉',
  processCycloneSeparator: '旋风分离器',
  processPreheaterTower: '预热/冷却器',
  processBagFilter: '布袋除尘器',
  processFanBlower: '引风机',
  processWinccFan: '风机',
  processBurner: '燃烧器',
  processAluminaSilo: '氧化铝料仓',
  processScrewConveyor: '螺旋输送机',
  processBucketElevator: '斗式提升机',
  processGasValveSkid: '燃气阀组',
  processStatusMatrix: '状态灯盘',
  processHeatExchanger: '换热器',
  processKilnTube: '炉筒子',
  processStack: '烟囱/排气筒',
  processRotaryAirlock: '星型下料阀',
  processDamper: '风门挡板',
  processSprayLance: '喷枪',
  processMaterialChute: '斜溜槽',
  processDoubleFlapValve: '双层翻板阀'
}
const displayTitle = computed(() => props.widget.config.title || typeLabels[props.widget.type] || props.widget.type)
const hideName = computed(() => Boolean(props.widget.config?.hideName))
const titleStyle = computed(() => {
  const size = Number(props.widget.config.titleFontSize)
  return { fontSize: (Number.isFinite(size) ? Math.max(8, Math.min(72, size)) : 12) + 'px' }
})
const isButtonWidget = computed(() => ['button', 'winccToggleButton', 'stepperControl'].includes(props.widget.type))
const isLabelWidget = computed(() => props.widget.type === 'label')
const isStatusCircleWidget = computed(() => props.widget.type === 'statusCircle')
const isFrameBoxWidget = computed(() => props.widget.type === 'frameBox')
const isCustomShapeWidget = computed(() => props.widget.type === 'customShape')
const isScadaSvgWidget = computed(() => props.widget.type === 'scadaSvg')
const isProcessValueTagWidget = computed(() => props.widget.type === 'processValueTag')
const isProcessWidget = computed(() => String(props.widget.type || '').startsWith('process'))
function isConnectableWidgetType(type) {
  const normalized = String(type || '')
  if (normalized === 'scadaSvg' || normalized === 'customShape') return true
  if (normalized.startsWith('process')) return !['processValueTag', 'processStatusMatrix'].includes(normalized)
  return false
}
const isConnectableWidget = computed(() => isConnectableWidgetType(props.widget.type))
const isFramelessWidget = computed(() => isButtonWidget.value || isProcessWidget.value || isCustomShapeWidget.value || isScadaSvgWidget.value || isLabelWidget.value || isStatusCircleWidget.value || isFrameBoxWidget.value)
const showConnectionPorts = computed(() =>
  !props.readonly &&
  isConnectableWidget.value &&
  (props.selected || props.connectionSource || props.forceShowPorts)
)
function normalizedConfiguredPort(port, index) {
  const side = ['top', 'right', 'bottom', 'left'].includes(port?.side) ? port.side : null
  if (!side) return null
  const x = Number(port.x)
  const y = Number(port.y)
  if (!Number.isFinite(x) || !Number.isFinite(y)) return null
  return {
    key: String(port.id || `${side}-${index}`),
    name: side,
    anchor: {
      x: Math.max(0, Math.min(1, x)),
      y: Math.max(0, Math.min(1, y)),
      side
    }
  }
}

function defaultPortNamesForWidget() {
  if (props.widget.type === 'processPump') return ['left', 'right']
  if (props.widget.type === 'processBagFilter') return ['right', 'left']
  if (props.widget.type === 'processFanBlower') return ['left', 'right', 'top']
  if (props.widget.type === 'processWinccFan') return ['top']
  if (props.widget.type === 'processHeatExchanger') return ['top', 'bottom']
  if (props.widget.type === 'processRotaryAirlock') return ['top']
  if (['processKilnTube', 'processDamper', 'processSprayLance', 'processMaterialChute'].includes(props.widget.type)) return ['left', 'right']
  if (props.widget.type === 'processStack') return ['bottom', 'left', 'right']
  return ['top', 'right', 'bottom', 'left']
}

const configuredConnectionPorts = computed(() => {
  const ports = Array.isArray(props.widget.config?.connectionPorts) ? props.widget.config.connectionPorts : []
  const normalized = ports.map(normalizedConfiguredPort).filter(Boolean)
  return normalized.map(port => ({
    ...port,
    connectionAnchor: port.anchor
  }))
})
const hasExplicitConnectionPorts = computed(() => Array.isArray(props.widget.config?.connectionPorts))
const configuredPortSides = computed(() => new Set(configuredConnectionPorts.value.map(port => port.name)))

const connectionPortNames = computed(() => defaultPortNamesForWidget())
const connectionPorts = computed(() => {
  const configured = configuredConnectionPorts.value
  const ports = hasExplicitConnectionPorts.value
    ? configured
    : connectionPortNames.value.map(name => ({ key: name, name, anchor: null, connectionAnchor: null }))
  return ports.map(port => {
    const point = port.anchor
      ? {
          x: Number(props.widget.x) + Number(props.widget.w) * port.anchor.x,
          y: Number(props.widget.y) + Number(props.widget.h) * port.anchor.y
        }
      : vectorPort(props.widget, port.name)
    const localX = point.x - props.widget.x
    const localY = point.y - props.widget.y
    return {
      ...port,
      style: {
        left: `${localX}px`,
        top: `${localY}px`
      },
      guideStyle: portGuideStyle(port.name, localX, localY)
    }
  })
})

function portGuideStyle(side, x, y) {
  const length = 18
  if (side === 'left') return { left: '0px', top: `${y}px`, width: `${Math.max(1, x + length)}px`, height: '2px', transform: 'translate(0, -50%)' }
  if (side === 'right') return { left: `${Math.min(x - length, Number(props.widget.w) || x)}px`, top: `${y}px`, width: `${length}px`, height: '2px', transform: 'translate(0, -50%)' }
  if (side === 'top') return { left: `${x}px`, top: '0px', width: '2px', height: `${Math.max(1, y + length)}px`, transform: 'translate(-50%, 0)' }
  return { left: `${x}px`, top: `${Math.min(y - length, Number(props.widget.h) || y)}px`, width: '2px', height: `${length}px`, transform: 'translate(-50%, 0)' }
}
const customConnectionPorts = computed(() => {
  const anchors = []
  const seen = new Set()
  const addAnchor = (anchor, label = '自定义连接端点') => {
    if (!anchor?.side) return
    const key = `${anchor.side}:${Number(anchor.x).toFixed(5)}:${Number(anchor.y).toFixed(5)}`
    if (seen.has(key)) return
    seen.add(key)
    anchors.push({
      key,
      name: anchor.side,
      anchor,
      title: label,
      style: {
        left: `${Number(anchor.x) * Number(props.widget.w)}px`,
        top: `${Number(anchor.y) * Number(props.widget.h)}px`
      }
    })
  }
  if (props.widget.type === 'processBagFilter' && (!hasExplicitConnectionPorts.value || configuredPortSides.value.has('bottom'))) {
    [
      { x: 64 / 246, y: 1, side: 'bottom' },
      { x: 106 / 246, y: 1, side: 'bottom' },
      { x: 148 / 246, y: 1, side: 'bottom' },
      { x: 190 / 246, y: 1, side: 'bottom' }
    ].forEach((anchor, index) => addAnchor(anchor, `布袋除尘器下料口 ${index + 1}`))
  }
  store.connections.forEach(connection => {
    const anchor = connection.from === props.widget.id
      ? connection.fromAnchor
      : (connection.to === props.widget.id ? connection.toAnchor : null)
    addAnchor(anchor)
  })
  return anchors
})

const widgetRotation = computed(() => {
  const value = Number(props.widget.config?.rotation)
  return Number.isFinite(value) ? value : 0
})

function clampStyleNumber(value, min, max, fallback = null) {
  const number = Number(value)
  if (!Number.isFinite(number)) return fallback
  return Math.max(min, Math.min(max, number))
}

function hexToRgb(color) {
  if (typeof color !== 'string') return null
  let hex = color.trim()
  if (!hex.startsWith('#')) return null
  hex = hex.slice(1)
  if (hex.length === 3) hex = hex.split('').map(char => char + char).join('')
  if (hex.length !== 6) return null
  const value = Number.parseInt(hex, 16)
  if (!Number.isFinite(value)) return null
  return {
    r: (value >> 16) & 255,
    g: (value >> 8) & 255,
    b: value & 255
  }
}

function colorWithOpacity(color, opacity = 1) {
  const rgb = hexToRgb(color)
  if (!rgb) return color || 'transparent'
  return `rgba(${rgb.r}, ${rgb.g}, ${rgb.b}, ${Math.max(0, Math.min(1, opacity))})`
}

function colorValue(value, fallback) {
  return typeof value === 'string' && /^#[0-9a-f]{3,6}$/i.test(value) ? value : fallback
}

function isActiveRuntimeValue(value) {
  if (typeof value === 'boolean') return value
  if (typeof value === 'number') return Number.isFinite(value) && value !== 0
  if (typeof value === 'string') {
    const normalized = value.trim().toLowerCase()
    if (!normalized) return false
    if (['false', 'off', 'stop', 'stopped', '0'].includes(normalized)) return false
    return true
  }
  return Boolean(value)
}

const visualStatusColor = computed(() => {
  const config = props.widget.config || {}
  if (!config.bindVariable || config.varId == null || config.varId === '') return null
  const dp = store.realtimeData[config.varId]
  if (!dp || dp.quality === 'BAD') return colorValue(config.statusOffColor, '#64748b')
  return isActiveRuntimeValue(dp.value)
    ? colorValue(config.statusOnColor, '#22c55e')
    : colorValue(config.statusOffColor, '#64748b')
})

const wrapperAppearanceStyle = computed(() => {
  const config = props.widget.config || {}
  const style = {}
  const opacity = clampStyleNumber(config.opacity, 0, 100, null)
  if (opacity != null && opacity < 100) style.opacity = opacity / 100

  const backgroundOpacity = clampStyleNumber(config.backgroundOpacity, 0, 100, null)
  if (backgroundOpacity != null && backgroundOpacity > 0) {
    style.background = colorWithOpacity(colorValue(config.background, '#ffffff'), backgroundOpacity / 100)
  }

  const borderRadius = clampStyleNumber(config.borderRadius, 0, 120, null)
  // The base wrapper has a theme radius.  A configured 0 must explicitly
  // override it; otherwise rectangle components still render as rounded.
  if (Object.prototype.hasOwnProperty.call(config, 'borderRadius')) {
    style.borderRadius = `${borderRadius ?? 0}px`
  }

  const configuredBorderWidth = clampStyleNumber(config.borderWidth, 0, 40, null)
  const highlightColor = visualStatusColor.value
  const borderWidth = highlightColor && (!configuredBorderWidth || configuredBorderWidth < 1)
    ? 1
    : configuredBorderWidth
  if (borderWidth != null && borderWidth > 0) {
    style.borderWidth = `${borderWidth}px`
    style.borderStyle = 'solid'
    style.borderColor = highlightColor || colorValue(config.borderColor, '#4f6fb8')
  } else if (highlightColor) {
    style.borderWidth = '1px'
    style.borderStyle = 'solid'
    style.borderColor = highlightColor
  }

  return style
})

// 像素级定位：x = left px, y = top px, w = width px, h = height px
const wrapperStyle = computed(() => ({
  left: Math.round(props.widget.x + (Number(props.canvasOffset?.x) || 0)) + 'px',
  top: Math.round(props.widget.y + (Number(props.canvasOffset?.y) || 0)) + 'px',
  width: Math.max(1, Math.round(props.widget.w)) + 'px',
  height: Math.max(1, Math.round(props.widget.h)) + 'px',
  ...wrapperAppearanceStyle.value,
  transform: widgetRotation.value ? `rotate(${widgetRotation.value}deg)` : undefined,
  transformOrigin: 'center center',
  zIndex: isFrameBoxWidget.value
    ? (isDragging.value || isResizing.value ? 1000 : (props.selected ? 1 : 0))
    : (isDragging.value || isResizing.value ? 1000 : 2)
}))
function isWidgetActionTarget(event) {
  return Boolean(event?.target?.closest?.('.ww-actions, .widget-action-btn'))
}

function onMouseDown(e) {
  if (isWidgetActionTarget(e)) return
  if (!props.readonly && isConnectableWidget.value && e.button === 0 && e.ctrlKey) {
    emit('select')
    emit('custom-port-connection-start', {
      widgetId: props.widget.id,
      event: e
    })
    return
  }
  if (!props.readonly && props.connectionMode && e.button === 0) {
    emit('select')
    return
  }
  if (!props.readonly) startDrag(e)
}

function startPortConnection(port, e, anchor = null) {
  emit('select')
  emit('port-connection-start', {
    widgetId: props.widget.id,
    port,
    anchor,
    event: e
  })
}


function startDrag(e) {
  if (e.button !== 0) return
  if (isWidgetActionTarget(e)) return
  emit('select')
  const startX = e.clientX
  const startY = e.clientY
  const origX = props.widget.x
  const origY = props.widget.y
  const z = Number(canvasZoom.value) || 1
  let dragFrame = 0
  let pendingPosition = null
  let dragStarted = false
  let transformStarted = false

  const flushMove = () => {
    dragFrame = 0
    if (!pendingPosition) return
    store.moveWidget(props.widget.id, pendingPosition.x, pendingPosition.y)
    pendingPosition = null
  }

  function onMove(ev) {
    if ((ev.buttons & 1) !== 1) {
      onUp()
      return
    }
    const clientDx = ev.clientX - startX
    const clientDy = ev.clientY - startY
    if (!dragStarted && Math.hypot(clientDx, clientDy) < DRAG_START_THRESHOLD) return
    if (!dragStarted) {
      dragStarted = true
      isDragging.value = true
      if (typeof beginWidgetTransform === 'function') {
        beginWidgetTransform(props.widget.id)
        transformStarted = true
      }
    }
    const dx = (ev.clientX - startX) / z
    const dy = (ev.clientY - startY) / z
    const nextX = origX + dx
    const nextY = origY + dy
    const aligned = typeof alignWidgetDrag === 'function'
      ? alignWidgetDrag(props.widget.id, nextX, nextY)
      : { x: nextX, y: nextY }
    pendingPosition = {
      x: Math.round(aligned.x),
      y: Math.round(aligned.y)
    }
    if (!dragFrame) dragFrame = requestAnimationFrame(flushMove)
  }
  function onUp() {
    isDragging.value = false
    if (dragFrame) {
      cancelAnimationFrame(dragFrame)
      dragFrame = 0
    }
    if (pendingPosition) {
      store.moveWidget(props.widget.id, pendingPosition.x, pendingPosition.y)
      pendingPosition = null
    }
    if (typeof clearWidgetDragAlignment === 'function') clearWidgetDragAlignment(props.widget.id)
    if (transformStarted && typeof endWidgetTransform === 'function') endWidgetTransform(props.widget.id)
    document.removeEventListener('mousemove', onMove)
    document.removeEventListener('mouseup', onUp)
    window.removeEventListener('blur', onUp)
  }
  document.addEventListener('mousemove', onMove)
  document.addEventListener('mouseup', onUp)
  window.addEventListener('blur', onUp)
}

function startResize(dir, event) {
  if (!event) return
  if (typeof beginWidgetTransform === 'function') beginWidgetTransform(props.widget.id)
  isResizing.value = true
  const startX = event.clientX
  const startY = event.clientY
  const origW = props.widget.w
  const origH = props.widget.h
  const aspectRatio = origH > 0 ? origW / origH : 1
  const z = canvasZoom.value
  let resizeFrame = 0
  let pendingSize = null

  const flushResize = () => {
    resizeFrame = 0
    if (!pendingSize) return
    store.resizeWidget(props.widget.id, pendingSize.w, pendingSize.h)
    pendingSize = null
  }

  function onMove(ev) {
    const dx = (ev.clientX - startX) / z
    const dy = (ev.clientY - startY) / z
    let nw = origW
    let nh = origH
    if (dir === 'se') {
      const scaleX = (origW + dx) / Math.max(origW, 1)
      const scaleY = (origH + dy) / Math.max(origH, 1)
      const dominantScale = Math.abs(scaleX - 1) >= Math.abs(scaleY - 1) ? scaleX : scaleY
      const minScale = Math.max(MIN_RESIZE_SIZE / Math.max(origW, 1), MIN_RESIZE_SIZE / Math.max(origH, 1))
      const uniformScale = Math.max(minScale, dominantScale)
      nw = Math.max(MIN_RESIZE_SIZE, origW * uniformScale)
      nh = Math.max(MIN_RESIZE_SIZE, aspectRatio > 0 ? nw / aspectRatio : origH * uniformScale)
    } else {
      if (dir === 'e') nw = Math.max(MIN_RESIZE_SIZE, origW + dx)
      if (dir === 's') nh = Math.max(MIN_RESIZE_SIZE, origH + dy)
    }
    pendingSize = { w: Math.round(nw), h: Math.round(nh) }
    if (!resizeFrame) resizeFrame = requestAnimationFrame(flushResize)
  }
  function onUp() {
    isResizing.value = false
    if (resizeFrame) {
      cancelAnimationFrame(resizeFrame)
      resizeFrame = 0
    }
    if (pendingSize) {
      store.resizeWidget(props.widget.id, pendingSize.w, pendingSize.h)
      pendingSize = null
    }
    if (typeof endWidgetTransform === 'function') endWidgetTransform(props.widget.id)
    document.removeEventListener('mousemove', onMove)
    document.removeEventListener('mouseup', onUp)
  }
  document.addEventListener('mousemove', onMove)
  document.addEventListener('mouseup', onUp)
}

function openConfig() { emit('open-config', props.widget) }
function remove() { emit('request-remove', props.widget) }
</script>

<style scoped>
.widget-wrapper {
  position: absolute;
  background: var(--bg-card);
  border: 1px solid var(--border-light);
  border-radius: var(--radius-md);
  display: flex;
  flex-direction: column;
  overflow: hidden;
  cursor: grab;
  box-shadow: var(--shadow-card);
  transition: box-shadow var(--transition-fast), border-color var(--transition-fast), opacity var(--transition-fast);
  text-rendering: geometricPrecision;
  -webkit-font-smoothing: antialiased;
}
.widget-wrapper.dragging,
.widget-wrapper.resizing {
  will-change: left, top, width, height;
}
.widget-wrapper:hover { box-shadow: var(--shadow-md); }
.widget-wrapper.selected {
  border-color: var(--widget-action-hover-text);
  box-shadow: var(--shadow-focus), var(--shadow-md);
  overflow: visible;
}
.widget-wrapper.button-shell {
  background: transparent;
  border-color: transparent;
  box-shadow: none;
  overflow: visible;
}
.widget-wrapper.button-shell:hover,
.widget-wrapper.process-shell:hover,
.widget-wrapper.custom-shell:hover,
.widget-wrapper.label-shell:hover,
.widget-wrapper.status-circle-shell:hover,
.widget-wrapper.scada-svg-shell:hover,
.widget-wrapper.frame-shell:hover {
  box-shadow: none;
}
.widget-wrapper.button-shell.selected,
.widget-wrapper.process-shell.selected,
.widget-wrapper.custom-shell.selected,
.widget-wrapper.label-shell.selected,
.widget-wrapper.status-circle-shell.selected,
.widget-wrapper.scada-svg-shell.selected,
.widget-wrapper.frame-shell.selected {
  border-color: var(--widget-action-hover-text);
  box-shadow: var(--shadow-focus);
}
.widget-wrapper.process-value-tag-shell.selected {
  border-color: transparent;
  box-shadow: none;
}
.widget-wrapper.label-shell,
.widget-wrapper.status-circle-shell,
.widget-wrapper.process-shell,
.widget-wrapper.custom-shell,
.widget-wrapper.scada-svg-shell,
.widget-wrapper.frame-shell {
  background: transparent;
  border-color: transparent;
  box-shadow: none;
  overflow: visible;
}
.widget-wrapper.frame-shell {
  pointer-events: none;
}
.widget-wrapper.frame-shell .ww-actions,
.widget-wrapper.frame-shell .resize-handle,
.widget-wrapper.frame-shell .frame-edge-hit {
  pointer-events: auto;
}
.widget-wrapper.frame-shell.readonly {
  pointer-events: none;
}
.widget-wrapper.process-shell.connection-mode {
  cursor: crosshair;
}
.widget-wrapper.process-shell.connection-source,
.widget-wrapper.connection-source {
  border-color: #1f8f5a;
  box-shadow: 0 0 0 3px rgba(31, 143, 90, 0.18);
}
.widget-wrapper.readonly {
  cursor: default;
}
.widget-wrapper.readonly:hover {
  box-shadow: var(--shadow-card);
}
.widget-wrapper.dragging {
  cursor: grabbing;
  z-index: 1000;
  box-shadow: var(--shadow-lg);
  opacity: 0.92;
}
.widget-wrapper.resizing {
  z-index: 1000;
  box-shadow: var(--shadow-lg);
}

.ww-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 6px 10px;
  background: var(--surface-muted);
  border-bottom: 1px solid var(--border-light);
  border-top: 2px solid var(--accent);
  flex-shrink: 0;
  min-height: 34px;
}
.ww-title {
  font-size: var(--fs-xs);
  font-weight: var(--fw-semibold);
  color: var(--text-primary);
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.ww-actions {
  display: flex;
  gap: 4px;
  opacity: 0;
  pointer-events: none;
  transition: opacity var(--transition-fast), transform var(--transition-fast);
}
.widget-wrapper.selected .ww-actions,
.ww-actions:focus-within {
  opacity: 1;
  pointer-events: none;
}
.widget-action-btn {
  flex: 0 0 auto;
  width: 24px;
  height: 24px;
  min-height: 24px;
  padding: 0;
  line-height: 1;
  font-size: 13px;
  color: var(--text-secondary);
  pointer-events: auto;
}
.ww-floating-actions {
  position: absolute;
  z-index: 1201;
  top: -48px;
  right: -4px;
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: max-content;
  min-width: 0;
  padding: 3px;
  background: var(--widget-action-bg);
  border: 1px solid var(--widget-action-border);
  border-radius: var(--radius-sm);
  box-shadow: var(--widget-action-shadow);
  transform: translateY(-2px);
  backdrop-filter: blur(8px);
  pointer-events: none;
}
.ww-floating-actions .widget-action-btn {
  color: var(--widget-action-text);
  border-color: transparent;
}
.ww-floating-actions .widget-action-btn:hover {
  background: var(--widget-action-hover-bg);
  color: var(--widget-action-hover-text);
}
.ww-floating-actions .widget-action-btn:last-child:hover {
  color: var(--danger-text);
}
.widget-wrapper.selected .ww-floating-actions,
.ww-floating-actions:focus-within {
  transform: translateY(-6px);
  pointer-events: none;
}

.ww-body { flex: 1; overflow: hidden; }
.ww-body-full {
  width: 100%;
  height: 100%;
  overflow: visible;
}

.frame-edge-hit {
  position: absolute;
  inset: -8px;
  z-index: 9;
  pointer-events: none;
}
.frame-edge {
  position: absolute;
  pointer-events: auto;
  background: transparent;
}
.frame-edge.edge-top {
  left: 0;
  right: 0;
  top: 0;
  height: 16px;
  cursor: move;
}
.frame-edge.edge-bottom {
  left: 0;
  right: 0;
  bottom: 0;
  height: 16px;
  cursor: move;
}
.frame-edge.edge-left {
  left: 0;
  top: 0;
  bottom: 0;
  width: 16px;
  cursor: move;
}
.frame-edge.edge-right {
  right: 0;
  top: 0;
  bottom: 0;
  width: 16px;
  cursor: move;
}

.resize-handle { position: absolute; z-index: 10; }
.resize-handle.br { right: -2px; bottom: -2px; width: 14px; height: 14px; cursor: nwse-resize; }
.resize-handle.b { left: 0; bottom: 0; width: 100%; height: 6px; cursor: ns-resize; }
.resize-handle.r { right: -10px; top: 0; width: 20px; height: 100%; cursor: ew-resize; }
.widget-wrapper.custom-shell .resize-handle.r::before,
.widget-wrapper.scada-svg-shell .resize-handle.r::before {
  content: '';
  position: absolute;
  left: 58%;
  top: 50%;
  width: 16px;
  height: 16px;
  border-radius: 999px;
  background: rgba(255,255,255,0.96);
  border: 2px solid rgba(79, 111, 184, 0.68);
  box-shadow: 0 2px 8px rgba(15, 23, 42, 0.16);
  transform: translate(-50%, -50%);
}
.widget-wrapper.custom-shell .resize-handle.r::after,
.widget-wrapper.scada-svg-shell .resize-handle.r::after {
  content: '';
  position: absolute;
  left: 58%;
  top: 50%;
  width: 8px;
  height: 8px;
  border-left: 2px solid #4f6fb8;
  border-right: 2px solid #4f6fb8;
  transform: translate(-50%, -50%);
}
.widget-wrapper.custom-shell.ports-visible .resize-handle.r::before,
.widget-wrapper.scada-svg-shell.ports-visible .resize-handle.r::before,
.widget-wrapper.custom-shell.ports-visible .resize-handle.r::after,
.widget-wrapper.scada-svg-shell.ports-visible .resize-handle.r::after {
  top: 28%;
  left: 64%;
}
.scale-handle {
  display: flex;
  align-items: center;
  justify-content: center;
  border-radius: 999px;
  background: var(--surface-raised);
  border: 1px solid rgba(79, 111, 184, 0.42);
  box-shadow: 0 1px 4px rgba(15, 23, 42, 0.14);
}
.scale-handle-icon {
  position: relative;
  width: 7px;
  height: 7px;
}
.scale-handle-icon::before,
.scale-handle-icon::after {
  content: '';
  position: absolute;
  inset: 0;
  border-top: 1.5px solid #4f6fb8;
  border-right: 1.5px solid #4f6fb8;
  transform-origin: center;
}
.scale-handle-icon::before {
  transform: rotate(0deg) translate(1px, -1px);
}
.scale-handle-icon::after {
  transform: rotate(180deg) translate(1px, -1px);
}
.connection-ports {
  position: absolute;
  inset: 0;
  z-index: 14;
  pointer-events: none;
}
.connection-port-anchor {
  position: absolute;
  z-index: 14;
  width: 9px;
  height: 9px;
  padding: 0;
  border: 1.5px solid rgba(14, 165, 233, 0.95);
  border-radius: 999px;
  background: var(--surface-raised);
  box-shadow: 0 0 0 2px rgba(14, 165, 233, 0.18), 0 1px 4px rgba(15, 23, 42, 0.18);
  transform: translate(-50%, -50%);
  cursor: crosshair;
  pointer-events: auto;
  transition: transform var(--transition-fast), background var(--transition-fast), box-shadow var(--transition-fast), border-color var(--transition-fast);
}
.connection-port-anchor:hover {
  background: #22c55e;
  border-color: #ffffff;
  box-shadow: 0 0 0 5px rgba(34, 197, 94, 0.28), 0 0 12px rgba(34, 197, 94, 0.5);
  transform: translate(-50%, -50%) scale(1.38);
}
.connection-port-anchor:active {
  background: #16a34a;
  transform: translate(-50%, -50%) scale(1.5);
}
.connection-port-anchor.custom-port-anchor {
  background: #16a34a;
  border-color: #ffffff;
  box-shadow: 0 0 0 2px rgba(22, 163, 74, 0.3), 0 1px 4px rgba(15, 23, 42, 0.2);
}
.connection-port-guide {
  position: absolute;
  z-index: 13;
  border-radius: 999px;
  background: rgba(14, 165, 233, 0.76);
  box-shadow: 0 0 0 1px rgba(255, 255, 255, 0.75);
  pointer-events: none;
}
</style>

