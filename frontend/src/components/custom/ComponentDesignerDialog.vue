<template>
  <div class="designer-overlay">
    <div class="designer-dialog">
      <header class="designer-header">
        <div>
          <h3>组件配置</h3>
          <p>自定义工业功能块</p>
        </div>
      </header>

      <div class="designer-body" :style="designerBodyStyle">
        <aside class="designer-panel designer-panel-left">
          <div class="form-group">
            <label>组件名称</label>
            <input v-model="draft.name" />
          </div>
          <div class="form-row two-col">
            <div class="form-group">
              <label>宽度</label>
              <input type="number" min="1" v-model.number="draft.width" />
            </div>
            <div class="form-group">
              <label>高度</label>
              <input type="number" min="1" v-model.number="draft.height" />
            </div>
          </div>

          <div class="config-section boundary-section">
            <div class="section-title">组件边界 / 裁剪</div>
            <div class="form-row two-col">
              <div class="form-group">
                <label>左</label>
                <input type="number" v-model.number="draft.cropX" />
              </div>
              <div class="form-group">
                <label>上</label>
                <input type="number" v-model.number="draft.cropY" />
              </div>
            </div>
            <div class="form-row two-col">
              <div class="form-group">
                <label>宽</label>
                <input type="number" min="1" v-model.number="draft.cropWidth" />
              </div>
              <div class="form-group">
                <label>高</label>
                <input type="number" min="1" v-model.number="draft.cropHeight" />
              </div>
            </div>
            <div class="designer-action-row">
              <button type="button" class="btn btn-sm" @click="applyCropBoundary">应用裁剪</button>
              <button type="button" class="btn btn-sm" @click="fitCropToShapes">适配图形</button>
              <button type="button" class="btn btn-sm" @click="resetCropBoundary">重置边界</button>
            </div>
          </div>

          <div class="shape-tool-groups">
            <section v-for="group in shapeToolGroups" :key="group.title" class="shape-tool-group">
              <div class="shape-tool-title">{{ group.title }}</div>
              <div class="designer-tool-grid">
                <button
                  v-for="tool in group.tools"
                  :key="tool.kind"
                  type="button"
                  class="btn btn-sm shape-tool-btn"
                  @click="addShape(tool.kind)"
                >
                  <span class="shape-tool-icon">{{ tool.icon }}</span>
                  <span>{{ tool.label }}</span>
                </button>
              </div>
            </section>
          </div>

          <div class="config-section">
            <div class="section-title">变量颜色</div>
            <label class="inline-check"><input type="checkbox" v-model="draft.bindVariable" /> 具备绑定变量</label>
            <div class="form-row two-col">
              <div class="form-group">
                <label>开颜色</label>
                <input type="color" v-model="draft.statusOnColor" />
              </div>
              <div class="form-group">
                <label>关颜色</label>
                <input type="color" v-model="draft.statusOffColor" />
              </div>
            </div>
          </div>

          <div class="designer-save-row">
            <button type="button" class="btn btn-primary designer-save" @click="saveComponent">
              {{ draft.id ? '保存修改' : '添加到库中' }}
            </button>
            <button type="button" class="btn btn-sm" @click="resetDraft">新建</button>
          </div>
          <div v-if="saveMessage" class="designer-note">{{ saveMessage }}</div>

          <div class="config-section saved-component-section">
            <div class="saved-component-header">
              <div>
                <div class="section-title">功能块管理</div>
                <div class="saved-component-count">
                  共 {{ store.customComponents.length }} 个，显示 {{ filteredCustomComponents.length }} 个
                </div>
              </div>
              <div class="saved-component-header-actions">
                <input
                  ref="importFileInput"
                  type="file"
                  accept=".json,application/json"
                  class="hidden-file-input"
                  @change="importComponentLibrary"
                />
                <button type="button" class="btn btn-sm" @click="triggerImport">导入</button>
                <button type="button" class="btn btn-sm" @click="exportComponentLibrary">导出</button>
                <button
                  v-if="customComponentSearch"
                  type="button"
                  class="btn btn-sm"
                  @click="customComponentSearch = ''"
                >
                  清空
                </button>
              </div>
            </div>
            <div class="saved-component-tools">
              <input v-model.trim="customComponentSearch" placeholder="搜索功能块名称" />
              <select v-model="customComponentSort">
                <option value="name-asc">名称 A-Z</option>
                <option value="name-desc">名称 Z-A</option>
                <option value="newest">最近创建</option>
                <option value="largest">尺寸最大</option>
              </select>
            </div>
            <div v-if="filteredCustomComponents.length === 0" class="designer-empty compact">
              {{ store.customComponents.length ? '没有匹配的功能块' : '暂无自定义功能块' }}
            </div>
            <div v-else class="saved-component-list">
              <div
                v-for="component in filteredCustomComponents"
                :key="component.id"
                class="saved-component-row"
                :class="{ active: draft.id === component.id }"
              >
                <div class="saved-component-meta">
                  <span class="saved-component-name" :title="component.name">{{ component.name }}</span>
                  <small>{{ componentSummary(component) }}</small>
                </div>
                <div class="saved-component-actions">
                  <button type="button" class="btn btn-sm" @click="editCustomComponent(component)">编辑</button>
                  <button type="button" class="btn btn-sm" @click="copyCustomComponent(component)">复制</button>
                  <button type="button" class="btn btn-sm btn-danger" @click="deleteCustomComponent(component)">删除</button>
                </div>
              </div>
            </div>
          </div>
        </aside>
        <div
          class="panel-resizer panel-resizer-left"
          title="拖动调整左侧栏宽度"
          @mousedown.prevent="startPanelResize('left', $event)"
        ></div>

        <main class="designer-stage-wrap">
          <svg
            ref="stageRef"
            class="designer-stage"
            :viewBox="`0 0 ${safeWidth} ${safeHeight}`"
            preserveAspectRatio="xMidYMid meet"
            @mousedown.self="selectedShapeId = null"
          >
            <defs>
              <pattern id="designerGrid" width="20" height="20" patternUnits="userSpaceOnUse">
                <path d="M20 0H0V20" fill="none" stroke="#e2e8f0" stroke-width="1" />
              </pattern>
            </defs>
            <rect :width="safeWidth" :height="safeHeight" fill="url(#designerGrid)" />
            <g
              v-for="shape in draft.shapes"
              :key="shape.id"
              :transform="shapeTransform(shape)"
              @mousedown.stop.prevent="startShapeDrag(shape, $event)"
            >
              <rect
                v-if="['rect', 'square', 'round-rect', 'capsule'].includes(shape.kind)"
                :x="shape.x"
                :y="shape.y"
                :width="Math.max(0, Number(shape.w) || 0)"
                :height="Math.max(0, Number(shape.h) || 0)"
                :rx="shape.kind === 'capsule' ? Math.max(0, shape.h / 2) : shape.radius"
                :fill="shape.fill"
                :stroke="shape.stroke"
                :stroke-width="shape.strokeWidth"
              />
              <ellipse
                v-else-if="shape.kind === 'circle' || shape.kind === 'ellipse'"
                :cx="shape.x + shape.w / 2"
                :cy="shape.y + shape.h / 2"
                :rx="Math.max(0, shape.w / 2)"
                :ry="Math.max(0, shape.h / 2)"
                :fill="shape.fill"
                :stroke="shape.stroke"
                :stroke-width="shape.strokeWidth"
              />
              <polygon
                v-else-if="polygonShapeKinds.includes(shape.kind)"
                :points="polygonPoints(shape)"
                :fill="shape.fill"
                :stroke="shape.stroke"
                :stroke-width="shape.strokeWidth"
              />
              <path
                v-else-if="pathShapeKinds.includes(shape.kind)"
                :d="shapePath(shape)"
                :fill="pathShapeFill(shape)"
                :stroke="shape.stroke"
                :stroke-width="Math.max(1, shape.strokeWidth)"
                stroke-linecap="round"
                stroke-linejoin="round"
              />
              <g v-else-if="threeDShapeKinds.includes(shape.kind)">
                <path
                  v-for="part in threeDShapeParts(shape, shape.fill)"
                  :key="part.key"
                  :d="part.d"
                  :fill="part.fill"
                  :fill-opacity="part.fillOpacity"
                  :stroke="part.stroke || shape.stroke"
                  :stroke-opacity="part.strokeOpacity"
                  :stroke-width="Math.max(1, shape.strokeWidth)"
                  stroke-linejoin="round"
                  stroke-linecap="round"
                />
              </g>
              <line
                v-else-if="shape.kind === 'line'"
                :x1="shape.x"
                :y1="shape.y + shape.h / 2"
                :x2="shape.x + shape.w"
                :y2="shape.y + shape.h / 2"
                :stroke="shape.stroke"
                :stroke-width="Math.max(1, shape.strokeWidth)"
                stroke-linecap="round"
              />
              <line
                v-else-if="shape.kind === 'line-v'"
                :x1="shape.x + shape.w / 2"
                :y1="shape.y"
                :x2="shape.x + shape.w / 2"
                :y2="shape.y + shape.h"
                :stroke="shape.stroke"
                :stroke-width="Math.max(1, shape.strokeWidth)"
                stroke-linecap="round"
              />
              <line
                v-else-if="shape.kind === 'slash'"
                :x1="shape.x"
                :y1="shape.y + shape.h"
                :x2="shape.x + shape.w"
                :y2="shape.y"
                :stroke="shape.stroke"
                :stroke-width="Math.max(1, shape.strokeWidth)"
                stroke-linecap="round"
              />
              <line
                v-else-if="shape.kind === 'backslash'"
                :x1="shape.x"
                :y1="shape.y"
                :x2="shape.x + shape.w"
                :y2="shape.y + shape.h"
                :stroke="shape.stroke"
                :stroke-width="Math.max(1, shape.strokeWidth)"
                stroke-linecap="round"
              />
              <g v-else-if="shape.kind === 'cross'">
                <line
                  :x1="shape.x"
                  :y1="shape.y + shape.h / 2"
                  :x2="shape.x + shape.w"
                  :y2="shape.y + shape.h / 2"
                  :stroke="shape.stroke"
                  :stroke-width="Math.max(1, shape.strokeWidth)"
                  stroke-linecap="round"
                />
                <line
                  :x1="shape.x + shape.w / 2"
                  :y1="shape.y"
                  :x2="shape.x + shape.w / 2"
                  :y2="shape.y + shape.h"
                  :stroke="shape.stroke"
                  :stroke-width="Math.max(1, shape.strokeWidth)"
                  stroke-linecap="round"
                />
              </g>
              <rect
                v-if="selectedShapeId === shape.id"
                class="designer-selection"
                :x="shape.x - 4"
                :y="shape.y - 4"
                :width="shape.w + 8"
                :height="shape.h + 8"
              />
              <rect
                v-if="selectedShapeId === shape.id"
                class="designer-resize-handle"
                :x="shape.x + shape.w - 5"
                :y="shape.y + shape.h - 5"
                width="10"
                height="10"
                rx="2"
                @mousedown.stop.prevent="startShapeResize(shape, $event)"
              />
              <line
                v-if="selectedShapeId === shape.id"
                class="designer-rotate-arm"
                :x1="shape.x + shape.w / 2"
                :y1="shape.y - 4"
                :x2="shape.x + shape.w / 2"
                :y2="shape.y - 26"
              />
              <circle
                v-if="selectedShapeId === shape.id"
                class="designer-rotate-handle"
                :cx="shape.x + shape.w / 2"
                :cy="shape.y - 31"
                r="6"
                @mousedown.stop.prevent="startShapeRotate(shape, $event)"
              />
            </g>
            <circle
              v-for="port in draft.ports"
              :key="port.id"
              class="designer-port"
              :cx="portPoint(port).x"
              :cy="portPoint(port).y"
              r="5"
              @mousedown.stop.prevent="startPortDrag(port, $event)"
            />
            <rect
              class="designer-boundary"
              :x="cropBoundary.x"
              :y="cropBoundary.y"
              :width="cropBoundary.w"
              :height="cropBoundary.h"
            />
          </svg>
        </main>
        <div
          class="panel-resizer panel-resizer-right"
          title="拖动调整右侧栏宽度"
          @mousedown.prevent="startPanelResize('right', $event)"
        ></div>

        <aside class="designer-panel designer-panel-right">
          <div class="section-title">图形属性</div>
          <template v-if="selectedShape">
            <div class="form-row two-col">
              <div class="form-group"><label>X</label><input type="number" v-model.number="selectedShape.x" /></div>
              <div class="form-group"><label>Y</label><input type="number" v-model.number="selectedShape.y" /></div>
            </div>
            <div class="form-row two-col">
              <div class="form-group"><label>宽</label><input type="number" min="0" v-model.number="selectedShape.w" /></div>
              <div class="form-group"><label>高</label><input type="number" min="0" v-model.number="selectedShape.h" /></div>
            </div>
            <div class="form-row two-col">
              <div class="form-group"><label>旋转角度</label><input type="number" step="1" v-model.number="selectedShape.rotate" @input="normalizeSelectedShapeRotation" /></div>
              <div class="form-group">
                <label>旋转</label>
                <button type="button" class="btn btn-sm full-width" @click="selectedShape.rotate = 0">归零</button>
              </div>
            </div>
            <div class="form-row two-col">
              <div class="form-group"><label>填充</label><input type="color" v-model="selectedShape.fill" /></div>
              <div class="form-group"><label>边框</label><input type="color" v-model="selectedShape.stroke" /></div>
            </div>
            <div class="form-row two-col">
              <div class="form-group"><label>线宽</label><input type="number" min="0" max="16" v-model.number="selectedShape.strokeWidth" /></div>
              <div class="form-group"><label>圆角</label><input type="number" min="0" max="80" v-model.number="selectedShape.radius" /></div>
            </div>
            <label class="inline-check"><input type="checkbox" v-model="selectedShape.bindStatus" /> 响应开关颜色</label>
            <div class="designer-action-row">
              <button type="button" class="btn btn-sm" @click="copySelectedShape">复制图形</button>
              <button type="button" class="btn btn-sm" :disabled="!copiedShape" @click="pasteShape">粘贴图形</button>
              <button type="button" class="btn btn-sm btn-danger" @click="removeSelectedShape">删除图形</button>
            </div>
          </template>
          <div v-else class="designer-empty">未选择图形</div>

          <div class="designer-port-header">
            <div class="section-title">连接端口</div>
            <button type="button" class="btn btn-sm" @click="addPort">新增端口</button>
          </div>
          <div class="designer-port-list">
            <div v-for="port in draft.ports" :key="port.id" class="designer-port-row">
              <select v-model="port.side">
                <option value="top">上</option>
                <option value="right">右</option>
                <option value="bottom">下</option>
                <option value="left">左</option>
              </select>
              <label class="port-percent-field">
                <span>X</span>
                <input type="number" min="0" max="100" v-model.number="port.x" @input="clampPortPosition(port)" />
              </label>
              <label class="port-percent-field">
                <span>Y</span>
                <input type="number" min="0" max="100" v-model.number="port.y" @input="clampPortPosition(port)" />
              </label>
              <button type="button" class="btn-icon" @click="removePort(port.id)">✕</button>
            </div>
          </div>
        </aside>
      </div>
    </div>
  </div>
</template>

<script setup>
import { computed, onMounted, onUnmounted, reactive, ref } from 'vue'
import { useMonitorStore } from '../../store/index.js'
import api from '../../api/index.js'

const store = useMonitorStore()
const stageRef = ref(null)
const selectedShapeId = ref(null)
const saveMessage = ref('')
const copiedShape = ref(null)
const customComponentSearch = ref('')
const customComponentSort = ref('name-asc')
const importFileInput = ref(null)
const leftPanelWidth = ref(readPanelWidth('left', 300))
const rightPanelWidth = ref(readPanelWidth('right', 320))

const draft = reactive({
  id: null,
  name: '自定义功能块',
  width: 180,
  height: 120,
  cropX: 0,
  cropY: 0,
  cropWidth: 180,
  cropHeight: 120,
  bindVariable: false,
  statusOnColor: '#22c55e',
  statusOffColor: '#ef4444',
  shapes: [
    { id: makeId('shape'), kind: 'rect', x: 24, y: 24, w: 132, h: 72, rotate: 0, fill: '#dbe4ea', stroke: '#263544', strokeWidth: 2, radius: 4, bindStatus: true }
  ],
  ports: [
    { id: makeId('port'), side: 'left', x: 0, y: 50 },
    { id: makeId('port'), side: 'right', x: 100, y: 50 }
  ]
})

const safeWidth = computed(() => Math.max(1, Number(draft.width) || 180))
const safeHeight = computed(() => Math.max(1, Number(draft.height) || 120))
const selectedShape = computed(() => draft.shapes.find(shape => shape.id === selectedShapeId.value) || null)
const cropBoundary = computed(() => normalizedCropBoundary())
const designerBodyStyle = computed(() => ({
  '--designer-left-panel-width': `${leftPanelWidth.value}px`,
  '--designer-right-panel-width': `${rightPanelWidth.value}px`
}))
const filteredCustomComponents = computed(() => {
  const keyword = customComponentSearch.value.trim().toLowerCase()
  const list = (store.customComponents || []).filter(component => {
    if (!keyword) return true
    return String(component.name || '').toLowerCase().includes(keyword)
  })
  return [...list].sort((a, b) => {
    if (customComponentSort.value === 'name-desc') {
      return String(b.name || '').localeCompare(String(a.name || ''), 'zh-CN')
    }
    if (customComponentSort.value === 'newest') return componentTimestamp(b) - componentTimestamp(a)
    if (customComponentSort.value === 'largest') {
      return ((Number(b.width) || 0) * (Number(b.height) || 0)) - ((Number(a.width) || 0) * (Number(a.height) || 0))
    }
    return String(a.name || '').localeCompare(String(b.name || ''), 'zh-CN')
  })
})
const shapeToolGroups = [
  {
    title: '线条',
    tools: [
      { kind: 'line', label: '直线', icon: '-' },
      { kind: 'line-v', label: '竖线', icon: '|' },
      { kind: 'slash', label: '斜线', icon: '/' },
      { kind: 'backslash', label: '反斜线', icon: '\\' },
      { kind: 'cross', label: '十字线', icon: '+' },
      { kind: 'arc', label: '弧线', icon: '⌒' }
    ]
  },
  {
    title: '平面图形',
    tools: [
      { kind: 'rect', label: '长方形', icon: '▭' },
      { kind: 'square', label: '正方形', icon: '□' },
      { kind: 'round-rect', label: '圆角矩形', icon: '▢' },
      { kind: 'capsule', label: '胶囊框', icon: '▭' },
      { kind: 'circle', label: '圆形', icon: '○' },
      { kind: 'ellipse', label: '椭圆', icon: '◯' },
      { kind: 'triangle', label: '三角形', icon: '△' },
      { kind: 'right-triangle', label: '直角三角形', icon: '◺' },
      { kind: 'trapezoid', label: '梯形', icon: '▱' },
      { kind: 'parallelogram', label: '平行四边形', icon: '▱' },
      { kind: 'diamond', label: '菱形', icon: '◇' },
      { kind: 'pentagon', label: '五边形', icon: '⬠' },
      { kind: 'hexagon', label: '六边形', icon: '⬡' },
      { kind: 'octagon', label: '八边形', icon: '⬢' },
      { kind: 'star', label: '星形', icon: '★' }
    ]
  },
  {
    title: '立体图形',
    tools: [
      { kind: 'cube', label: '立方体', icon: '◇' },
      { kind: 'cylinder', label: '圆柱', icon: '◉' },
      { kind: 'cylinder-h', label: '卧式圆柱', icon: '◉' },
      { kind: 'cone', label: '圆锥', icon: '△' },
      { kind: 'frustum', label: '锥台/料斗', icon: '▽' },
      { kind: 'tri-prism', label: '三棱柱', icon: '△' }
    ]
  },
  {
    title: '流程 / 管道',
    tools: [
      { kind: 'arrow-left', label: '左箭头', icon: '←' },
      { kind: 'arrow-right', label: '右箭头', icon: '→' },
      { kind: 'chevron', label: '折向箭头', icon: '>' },
      { kind: 'pipe-elbow', label: '弯头', icon: '⌝' },
      { kind: 'pipe-tee', label: '三通', icon: '┬' }
    ]
  }
]
const lineShapeKinds = ['line', 'line-v', 'slash', 'backslash', 'cross', 'arc', 'pipe-elbow', 'pipe-tee']
const arrowShapeKinds = ['arrow-right', 'arrow-left']
const pathShapeKinds = ['arrow-right', 'arrow-left', 'arc', 'pipe-elbow', 'pipe-tee']
const filledPathShapeKinds = ['arrow-right', 'arrow-left']
const threeDShapeKinds = ['cube', 'cylinder', 'cylinder-h', 'cone', 'frustum', 'tri-prism']
const polygonShapeKinds = ['triangle', 'right-triangle', 'trapezoid', 'parallelogram', 'diamond', 'pentagon', 'hexagon', 'octagon', 'star', 'chevron']
const compactShapeKinds = ['circle', 'diamond', 'pentagon', 'hexagon', 'octagon', 'star', 'cone']

function makeId(prefix) {
  return `${prefix}-${Date.now()}-${Math.round(Math.random() * 100000)}`
}

function clampPanelWidth(value) {
  return Math.round(Math.max(220, Math.min(520, Number(value) || 0)))
}

function readPanelWidth(side, fallback) {
  try {
    const value = Number(localStorage.getItem(`monitor-designer-${side}-panel-width`))
    return Number.isFinite(value) ? clampPanelWidth(value) : fallback
  } catch {
    return fallback
  }
}

function persistPanelWidth(side, value) {
  try {
    localStorage.setItem(`monitor-designer-${side}-panel-width`, String(value))
  } catch {}
}

function startPanelResize(side, event) {
  const startX = event.clientX
  const origin = side === 'left' ? leftPanelWidth.value : rightPanelWidth.value
  document.body.classList.add('resizing-designer-panel')
  const onMove = (ev) => {
    const delta = ev.clientX - startX
    const next = clampPanelWidth(side === 'left' ? origin + delta : origin - delta)
    if (side === 'left') leftPanelWidth.value = next
    else rightPanelWidth.value = next
  }
  const onUp = () => {
    document.body.classList.remove('resizing-designer-panel')
    document.removeEventListener('mousemove', onMove)
    document.removeEventListener('mouseup', onUp)
    persistPanelWidth(side, side === 'left' ? leftPanelWidth.value : rightPanelWidth.value)
  }
  document.addEventListener('mousemove', onMove)
  document.addEventListener('mouseup', onUp)
}

function componentTimestamp(component) {
  const matched = String(component?.id || '').match(/(\d{10,})/)
  return matched ? Number(matched[1]) : 0
}

function componentSummary(component) {
  const width = Math.max(1, Number(component?.width) || 0)
  const height = Math.max(1, Number(component?.height) || 0)
  const shapeCount = Array.isArray(component?.shapes) ? component.shapes.length : 0
  const portCount = Array.isArray(component?.ports) ? component.ports.length : 0
  return `${width}×${height} · ${shapeCount}图形 · ${portCount}端口`
}

function defaultShape() {
  return { id: makeId('shape'), kind: 'rect', x: 24, y: 24, w: 132, h: 72, rotate: 0, fill: '#dbe4ea', stroke: '#263544', strokeWidth: 2, radius: 4, bindStatus: true }
}

function resetDraft() {
  draft.id = null
  draft.name = '自定义功能块'
  draft.width = 180
  draft.height = 120
  draft.cropX = 0
  draft.cropY = 0
  draft.cropWidth = 180
  draft.cropHeight = 120
  draft.bindVariable = false
  draft.statusOnColor = '#22c55e'
  draft.statusOffColor = '#ef4444'
  draft.shapes = [defaultShape()]
  draft.ports = [
    { id: makeId('port'), side: 'left', x: 0, y: 50 },
    { id: makeId('port'), side: 'right', x: 100, y: 50 }
  ]
  selectedShapeId.value = draft.shapes[0]?.id || null
  saveMessage.value = ''
}

function roundOne(value) {
  return Math.round((Number(value) || 0) * 10) / 10
}

function normalizedCropBoundary() {
  const x = Number.isFinite(Number(draft.cropX)) ? Number(draft.cropX) : 0
  const y = Number.isFinite(Number(draft.cropY)) ? Number(draft.cropY) : 0
  const w = Number.isFinite(Number(draft.cropWidth)) ? Number(draft.cropWidth) : safeWidth.value
  const h = Number.isFinite(Number(draft.cropHeight)) ? Number(draft.cropHeight) : safeHeight.value
  return {
    x: roundOne(x),
    y: roundOne(y),
    w: Math.max(1, roundOne(w)),
    h: Math.max(1, roundOne(h))
  }
}

function resetCropBoundary() {
  draft.cropX = 0
  draft.cropY = 0
  draft.cropWidth = safeWidth.value
  draft.cropHeight = safeHeight.value
}

function addShape(kind) {
  const size = lineShapeKinds.includes(kind)
    ? { w: 90, h: 44 }
    : { w: compactShapeKinds.includes(kind) ? 64 : 88, h: compactShapeKinds.includes(kind) ? 64 : 58 }
  const solidArrow = arrowShapeKinds.includes(kind)
  const isRoundedRect = kind === 'round-rect'
  const isCapsule = kind === 'capsule'
  const shape = {
    id: makeId('shape'),
    kind,
    x: Math.max(8, Math.round(safeWidth.value / 2 - size.w / 2)),
    y: Math.max(8, Math.round(safeHeight.value / 2 - size.h / 2)),
    ...size,
    fill: lineShapeKinds.includes(kind) ? 'transparent' : (solidArrow ? '#263544' : '#dbe4ea'),
    stroke: solidArrow ? '#263544' : '#263544',
    strokeWidth: 2,
    radius: isCapsule ? 999 : (isRoundedRect ? 14 : (kind === 'rect' ? 4 : 0)),
    rotate: 0,
    bindStatus: !lineShapeKinds.includes(kind)
  }
  draft.shapes.push(shape)
  selectedShapeId.value = shape.id
}

function regularPolygonPoints(shape, sides, rotationDeg = -90) {
  const cx = shape.x + shape.w / 2
  const cy = shape.y + shape.h / 2
  const rx = Math.max(0, shape.w / 2)
  const ry = Math.max(0, shape.h / 2)
  const points = []
  for (let i = 0; i < sides; i++) {
    const angle = ((360 / sides) * i + rotationDeg) * Math.PI / 180
    points.push(`${cx + Math.cos(angle) * rx},${cy + Math.sin(angle) * ry}`)
  }
  return points.join(' ')
}

function polygonPoints(shape) {
  if (shape.kind === 'triangle') {
    return `${shape.x + shape.w / 2},${shape.y} ${shape.x + shape.w},${shape.y + shape.h} ${shape.x},${shape.y + shape.h}`
  }
  if (shape.kind === 'right-triangle') {
    return `${shape.x},${shape.y} ${shape.x + shape.w},${shape.y + shape.h} ${shape.x},${shape.y + shape.h}`
  }
  if (shape.kind === 'trapezoid') {
    return `${shape.x + shape.w * 0.22},${shape.y} ${shape.x + shape.w * 0.78},${shape.y} ${shape.x + shape.w},${shape.y + shape.h} ${shape.x},${shape.y + shape.h}`
  }
  if (shape.kind === 'parallelogram') {
    return `${shape.x + shape.w * 0.24},${shape.y} ${shape.x + shape.w},${shape.y} ${shape.x + shape.w * 0.76},${shape.y + shape.h} ${shape.x},${shape.y + shape.h}`
  }
  if (shape.kind === 'diamond') {
    return `${shape.x + shape.w / 2},${shape.y} ${shape.x + shape.w},${shape.y + shape.h / 2} ${shape.x + shape.w / 2},${shape.y + shape.h} ${shape.x},${shape.y + shape.h / 2}`
  }
  if (shape.kind === 'chevron') {
    return `${shape.x},${shape.y} ${shape.x + shape.w * 0.72},${shape.y} ${shape.x + shape.w},${shape.y + shape.h / 2} ${shape.x + shape.w * 0.72},${shape.y + shape.h} ${shape.x},${shape.y + shape.h} ${shape.x + shape.w * 0.28},${shape.y + shape.h / 2}`
  }
  if (shape.kind === 'star') return starPoints(shape)
  if (shape.kind === 'pentagon') return regularPolygonPoints(shape, 5, -90)
  if (shape.kind === 'octagon') return regularPolygonPoints(shape, 8, 22.5)
  return regularPolygonPoints(shape, 6, 30)
}

function starPoints(shape) {
  const cx = shape.x + shape.w / 2
  const cy = shape.y + shape.h / 2
  const outerX = Math.max(0, shape.w / 2)
  const outerY = Math.max(0, shape.h / 2)
  const innerX = outerX * 0.42
  const innerY = outerY * 0.42
  const points = []
  for (let i = 0; i < 10; i++) {
    const angle = (-90 + i * 36) * Math.PI / 180
    const rx = i % 2 === 0 ? outerX : innerX
    const ry = i % 2 === 0 ? outerY : innerY
    points.push(`${cx + Math.cos(angle) * rx},${cy + Math.sin(angle) * ry}`)
  }
  return points.join(' ')
}

function pathShapeFill(shape) {
  return filledPathShapeKinds.includes(shape.kind) ? (shape.fill || '#263544') : 'none'
}

function ellipsePath(cx, cy, rx, ry) {
  return `M${cx - rx} ${cy}A${rx} ${ry} 0 1 0 ${cx + rx} ${cy}A${rx} ${ry} 0 1 0 ${cx - rx} ${cy}Z`
}

function pointsPath(points) {
  return points.map((point, index) => `${index === 0 ? 'M' : 'L'}${point[0]} ${point[1]}`).join('') + 'Z'
}

function threeDShapeParts(shape, fillColor = shape.fill || '#dbe4ea') {
  const x = Number(shape.x) || 0
  const y = Number(shape.y) || 0
  const w = Math.max(0, Number(shape.w) || 0)
  const h = Math.max(0, Number(shape.h) || 0)
  const depth = Math.max(8, Math.min(w, h) * 0.22)
  const fill = fillColor || '#dbe4ea'
  if (shape.kind === 'cylinder') {
    const rx = w / 2
    const ry = Math.max(2, Math.min(h * 0.12, 18))
    const cx = x + rx
    const topY = y + ry
    const bottomY = y + h - ry
    return [
      { key: 'body', d: `M${x} ${topY}L${x} ${bottomY}A${rx} ${ry} 0 0 0 ${x + w} ${bottomY}L${x + w} ${topY}A${rx} ${ry} 0 0 1 ${x} ${topY}Z`, fill, fillOpacity: 0.86 },
      { key: 'top', d: ellipsePath(cx, topY, rx, ry), fill: '#ffffff', fillOpacity: 0.36 },
      { key: 'bottom', d: `M${x} ${bottomY}A${rx} ${ry} 0 0 0 ${x + w} ${bottomY}`, fill: 'none', strokeOpacity: 0.65 }
    ]
  }
  if (shape.kind === 'cylinder-h') {
    const rx = Math.max(2, Math.min(w * 0.12, 18))
    const ry = h / 2
    const cy = y + ry
    const leftX = x + rx
    const rightX = x + w - rx
    return [
      { key: 'body', d: `M${leftX} ${y}H${rightX}A${rx} ${ry} 0 0 1 ${rightX} ${y + h}H${leftX}A${rx} ${ry} 0 0 0 ${leftX} ${y}Z`, fill, fillOpacity: 0.86 },
      { key: 'left', d: ellipsePath(leftX, cy, rx, ry), fill: '#ffffff', fillOpacity: 0.28 },
      { key: 'right', d: ellipsePath(rightX, cy, rx, ry), fill: '#ffffff', fillOpacity: 0.18 }
    ]
  }
  if (shape.kind === 'cube') {
    const d = Math.min(depth, w * 0.34, h * 0.34)
    return [
      { key: 'front', d: pointsPath([[x, y + d], [x + w - d, y + d], [x + w - d, y + h], [x, y + h]]), fill, fillOpacity: 0.9 },
      { key: 'top', d: pointsPath([[x, y + d], [x + d, y], [x + w, y], [x + w - d, y + d]]), fill: '#ffffff', fillOpacity: 0.32 },
      { key: 'side', d: pointsPath([[x + w - d, y + d], [x + w, y], [x + w, y + h - d], [x + w - d, y + h]]), fill: '#0f172a', fillOpacity: 0.12 }
    ]
  }
  if (shape.kind === 'cone') {
    const rx = w / 2
    const ry = Math.max(2, Math.min(h * 0.1, 16))
    const cx = x + rx
    const baseY = y + h - ry
    return [
      { key: 'body', d: `M${cx} ${y}L${x} ${baseY}A${rx} ${ry} 0 0 0 ${x + w} ${baseY}Z`, fill, fillOpacity: 0.88 },
      { key: 'base', d: ellipsePath(cx, baseY, rx, ry), fill: '#ffffff', fillOpacity: 0.25 }
    ]
  }
  if (shape.kind === 'frustum') {
    const topW = w * 0.7
    const bottomW = w * 0.34
    const topX = x + (w - topW) / 2
    const bottomX = x + (w - bottomW) / 2
    const topRy = Math.max(2, Math.min(h * 0.09, 13))
    const bottomRy = Math.max(2, Math.min(h * 0.07, 10))
    const topY = y + topRy
    const bottomY = y + h - bottomRy
    return [
      { key: 'body', d: pointsPath([[topX, topY], [topX + topW, topY], [bottomX + bottomW, bottomY], [bottomX, bottomY]]), fill, fillOpacity: 0.88 },
      { key: 'top', d: ellipsePath(x + w / 2, topY, topW / 2, topRy), fill: '#ffffff', fillOpacity: 0.32 },
      { key: 'bottom', d: ellipsePath(x + w / 2, bottomY, bottomW / 2, bottomRy), fill: '#ffffff', fillOpacity: 0.2 }
    ]
  }
  const d = Math.min(depth, w * 0.28, h * 0.28)
  return [
    { key: 'front', d: pointsPath([[x, y + h], [x + w * 0.5, y + d], [x + w - d, y + h]]), fill, fillOpacity: 0.9 },
    { key: 'top', d: pointsPath([[x + w * 0.5, y + d], [x + w * 0.5 + d, y], [x + w, y + h - d], [x + w - d, y + h]]), fill: '#ffffff', fillOpacity: 0.28 },
    { key: 'side', d: pointsPath([[x + w - d, y + h], [x + w, y + h - d], [x + d, y + h - d], [x, y + h]]), fill: '#0f172a', fillOpacity: 0.1 }
  ]
}

function shapePath(shape) {
  const x = Number(shape.x) || 0
  const y = Number(shape.y) || 0
  const w = Math.max(0, Number.isFinite(Number(shape.w)) ? Number(shape.w) : 0)
  const h = Math.max(0, Number.isFinite(Number(shape.h)) ? Number(shape.h) : 0)
  if (shape.kind === 'arrow-left') {
    return `M${x + w} ${y + h * 0.25}H${x + w * 0.35}V${y}L${x} ${y + h * 0.5}L${x + w * 0.35} ${y + h}V${y + h * 0.75}H${x + w}Z`
  }
  if (shape.kind === 'arc') {
    return `M${x} ${y + h}C${x + w * 0.25} ${y},${x + w * 0.75} ${y},${x + w} ${y + h}`
  }
  if (shape.kind === 'pipe-elbow') {
    return `M${x + w * 0.18} ${y + h}V${y + h * 0.38}Q${x + w * 0.18} ${y + h * 0.18},${x + w * 0.38} ${y + h * 0.18}H${x + w}`
  }
  if (shape.kind === 'pipe-tee') {
    return `M${x} ${y + h * 0.28}H${x + w}M${x + w * 0.5} ${y + h * 0.28}V${y + h}`
  }
  return `M${x} ${y + h * 0.25}H${x + w * 0.65}V${y}L${x + w} ${y + h * 0.5}L${x + w * 0.65} ${y + h}V${y + h * 0.75}H${x}Z`
}

function normalizeRotation(value) {
  const number = Number(value)
  if (!Number.isFinite(number)) return 0
  return Math.round((((number % 360) + 360) % 360) * 10) / 10
}

function shapeRotation(shape) {
  return Number.isFinite(Number(shape.rotate)) ? Number(shape.rotate) : 0
}

function shapeCenter(shape) {
  return {
    x: (Number(shape.x) || 0) + (Number(shape.w) || 0) / 2,
    y: (Number(shape.y) || 0) + (Number(shape.h) || 0) / 2
  }
}

function rotatedShapeBounds(shape) {
  const x = Number(shape.x) || 0
  const y = Number(shape.y) || 0
  const w = Math.max(0, Number(shape.w) || 0)
  const h = Math.max(0, Number(shape.h) || 0)
  const pad = Math.max(0, Number(shape.strokeWidth) || 0) / 2
  const minX = x - pad
  const minY = y - pad
  const maxX = x + w + pad
  const maxY = y + h + pad
  const rotate = shapeRotation(shape) * Math.PI / 180
  if (!rotate) return { minX, minY, maxX, maxY }
  const center = shapeCenter(shape)
  const cos = Math.cos(rotate)
  const sin = Math.sin(rotate)
  const points = [
    { x: minX, y: minY },
    { x: maxX, y: minY },
    { x: maxX, y: maxY },
    { x: minX, y: maxY }
  ].map(point => {
    const dx = point.x - center.x
    const dy = point.y - center.y
    return {
      x: center.x + dx * cos - dy * sin,
      y: center.y + dx * sin + dy * cos
    }
  })
  return {
    minX: Math.min(...points.map(point => point.x)),
    minY: Math.min(...points.map(point => point.y)),
    maxX: Math.max(...points.map(point => point.x)),
    maxY: Math.max(...points.map(point => point.y))
  }
}

function fitCropToShapes() {
  if (!draft.shapes.length) {
    resetCropBoundary()
    return
  }
  const bounds = draft.shapes
    .map(rotatedShapeBounds)
    .reduce((acc, item) => ({
      minX: Math.min(acc.minX, item.minX),
      minY: Math.min(acc.minY, item.minY),
      maxX: Math.max(acc.maxX, item.maxX),
      maxY: Math.max(acc.maxY, item.maxY)
    }), { minX: Infinity, minY: Infinity, maxX: -Infinity, maxY: -Infinity })
  if (!Number.isFinite(bounds.minX) || !Number.isFinite(bounds.minY) ||
      !Number.isFinite(bounds.maxX) || !Number.isFinite(bounds.maxY)) {
    resetCropBoundary()
    return
  }
  draft.cropX = Math.floor(bounds.minX)
  draft.cropY = Math.floor(bounds.minY)
  draft.cropWidth = Math.max(1, Math.ceil(bounds.maxX - bounds.minX))
  draft.cropHeight = Math.max(1, Math.ceil(bounds.maxY - bounds.minY))
}

function normalizedPortsForSave() {
  return draft.ports.map(port => {
    const position = normalizePortPosition(port)
    return {
      id: port.id || makeId('port'),
      side: ['top', 'right', 'bottom', 'left'].includes(port.side) ? port.side : 'right',
      x: position.x,
      y: position.y
    }
  })
}

function componentPayloadFromDraft() {
  const canvasWidth = safeWidth.value
  const canvasHeight = safeHeight.value
  const crop = normalizedCropBoundary()
  const shapes = JSON.parse(JSON.stringify(draft.shapes)).map(shape => ({
    ...shape,
    x: roundOne((Number(shape.x) || 0) - crop.x),
    y: roundOne((Number(shape.y) || 0) - crop.y)
  }))
  const ports = normalizedPortsForSave().map(port => {
    const absoluteX = canvasWidth * port.x / 100
    const absoluteY = canvasHeight * port.y / 100
    return {
      ...port,
      x: clampPercent((absoluteX - crop.x) / crop.w * 100, port.x),
      y: clampPercent((absoluteY - crop.y) / crop.h * 100, port.y)
    }
  })
  return {
    width: Math.max(1, Math.round(crop.w)),
    height: Math.max(1, Math.round(crop.h)),
    shapes,
    ports
  }
}

function applyCropBoundary() {
  const payload = componentPayloadFromDraft()
  draft.width = payload.width
  draft.height = payload.height
  draft.shapes = payload.shapes
  draft.ports = payload.ports
  resetCropBoundary()
  saveMessage.value = '已应用组件边界'
  window.setTimeout(() => { saveMessage.value = '' }, 1600)
}

function shapeTransform(shape) {
  const rotate = shapeRotation(shape)
  if (!rotate) return ''
  const center = shapeCenter(shape)
  return `rotate(${rotate} ${center.x} ${center.y})`
}

function normalizeSelectedShapeRotation() {
  if (!selectedShape.value) return
  selectedShape.value.rotate = normalizeRotation(selectedShape.value.rotate)
}

function removeSelectedShape() {
  if (!selectedShape.value) return
  draft.shapes = draft.shapes.filter(shape => shape.id !== selectedShape.value.id)
  selectedShapeId.value = draft.shapes[0]?.id || null
}

function copySelectedShape() {
  if (!selectedShape.value) return
  copiedShape.value = JSON.parse(JSON.stringify(selectedShape.value))
}

function pasteShape() {
  if (!copiedShape.value) return
  const shape = {
    ...JSON.parse(JSON.stringify(copiedShape.value)),
    id: makeId('shape'),
    x: Math.round((Number(copiedShape.value.x) || 0) + 16),
    y: Math.round((Number(copiedShape.value.y) || 0) + 16)
  }
  draft.shapes.push(shape)
  selectedShapeId.value = shape.id
  copiedShape.value = JSON.parse(JSON.stringify(shape))
}

function addPort() {
  draft.ports.push({ id: makeId('port'), side: 'right', x: 100, y: 50 })
}

function removePort(id) {
  draft.ports = draft.ports.filter(port => port.id !== id)
}


function clampPercent(value, fallback = 50) {
  const number = Number(value)
  return Math.round(Math.max(0, Math.min(100, Number.isFinite(number) ? number : fallback)))
}

function normalizePortPosition(port) {
  if (Number.isFinite(Number(port.x)) && Number.isFinite(Number(port.y))) {
    return { x: clampPercent(port.x), y: clampPercent(port.y) }
  }
  const offset = clampPercent(port.offset, 50)
  if (port.side === 'top') return { x: offset, y: 0 }
  if (port.side === 'bottom') return { x: offset, y: 100 }
  if (port.side === 'left') return { x: 0, y: offset }
  return { x: 100, y: offset }
}

function clampPortPosition(port) {
  const position = normalizePortPosition(port)
  port.x = position.x
  port.y = position.y
}

function portPoint(port) {
  const position = normalizePortPosition(port)
  return {
    x: safeWidth.value * position.x / 100,
    y: safeHeight.value * position.y / 100
  }
}

function stagePoint(event) {
  const svg = stageRef.value
  if (svg?.createSVGPoint) {
    const point = svg.createSVGPoint()
    point.x = event.clientX
    point.y = event.clientY
    const matrix = svg.getScreenCTM()
    if (matrix) {
      const local = point.matrixTransform(matrix.inverse())
      return { x: local.x, y: local.y }
    }
  }
  const rect = svg?.getBoundingClientRect()
  if (!rect) return { x: 0, y: 0 }
  return {
    x: ((event.clientX - rect.left) / Math.max(1, rect.width)) * safeWidth.value,
    y: ((event.clientY - rect.top) / Math.max(1, rect.height)) * safeHeight.value
  }
}

function startShapeDrag(shape, event) {
  selectedShapeId.value = shape.id
  const start = stagePoint(event)
  const origin = { x: Number(shape.x) || 0, y: Number(shape.y) || 0 }
  const onMove = (ev) => {
    const point = stagePoint(ev)
    shape.x = Math.round(origin.x + point.x - start.x)
    shape.y = Math.round(origin.y + point.y - start.y)
  }
  const onUp = () => {
    document.removeEventListener('mousemove', onMove)
    document.removeEventListener('mouseup', onUp)
  }
  document.addEventListener('mousemove', onMove)
  document.addEventListener('mouseup', onUp)
}

function startShapeResize(shape, event) {
  selectedShapeId.value = shape.id
  const start = stagePoint(event)
  const origin = {
    w: Math.max(0, Number.isFinite(Number(shape.w)) ? Number(shape.w) : 0),
    h: Math.max(0, Number.isFinite(Number(shape.h)) ? Number(shape.h) : 0)
  }
  const rotate = -shapeRotation(shape) * Math.PI / 180
  const onMove = (ev) => {
    const point = stagePoint(ev)
    const dx = point.x - start.x
    const dy = point.y - start.y
    const localDx = dx * Math.cos(rotate) - dy * Math.sin(rotate)
    const localDy = dx * Math.sin(rotate) + dy * Math.cos(rotate)
    const nextW = Math.max(0, origin.w + localDx)
    const nextH = Math.max(0, origin.h + localDy)
    shape.w = Math.round(nextW)
    shape.h = Math.round(nextH)
  }
  const onUp = () => {
    document.removeEventListener('mousemove', onMove)
    document.removeEventListener('mouseup', onUp)
  }
  document.addEventListener('mousemove', onMove)
  document.addEventListener('mouseup', onUp)
}

function startShapeRotate(shape, event) {
  selectedShapeId.value = shape.id
  const center = shapeCenter(shape)
  const start = stagePoint(event)
  const startAngle = Math.atan2(start.y - center.y, start.x - center.x) * 180 / Math.PI
  const originRotate = shapeRotation(shape)
  const onMove = (ev) => {
    const point = stagePoint(ev)
    const angle = Math.atan2(point.y - center.y, point.x - center.x) * 180 / Math.PI
    shape.rotate = normalizeRotation(originRotate + angle - startAngle)
  }
  const onUp = () => {
    document.removeEventListener('mousemove', onMove)
    document.removeEventListener('mouseup', onUp)
  }
  document.addEventListener('mousemove', onMove)
  document.addEventListener('mouseup', onUp)
}

function startPortDrag(port, event) {
  const onMove = (ev) => {
    const point = stagePoint(ev)
    const xPercent = clampPercent(point.x / safeWidth.value * 100)
    const yPercent = clampPercent(point.y / safeHeight.value * 100)
    const distances = [
      { side: 'top', distance: Math.abs(point.y) },
      { side: 'right', distance: Math.abs(safeWidth.value - point.x) },
      { side: 'bottom', distance: Math.abs(safeHeight.value - point.y) },
      { side: 'left', distance: Math.abs(point.x) }
    ].sort((a, b) => a.distance - b.distance)
    const side = distances[0].side
    port.side = side
    port.x = xPercent
    port.y = yPercent
  }
  const onUp = () => {
    document.removeEventListener('mousemove', onMove)
    document.removeEventListener('mouseup', onUp)
  }
  onMove(event)
  document.addEventListener('mousemove', onMove)
  document.addEventListener('mouseup', onUp)
}

function deleteCustomComponent(component) {
  if (!component?.id) return
  store.removeCustomComponent(component.id)
  if (draft.id === component.id) draft.id = null
  saveMessage.value = `已删除「${component.name}」`
  window.setTimeout(() => { saveMessage.value = '' }, 2200)
}

function copyCustomComponent(component) {
  if (!component) return
  const copied = JSON.parse(JSON.stringify(component))
  const saved = store.saveCustomComponent({
    ...copied,
    id: makeId('custom'),
    name: `${copied.name || '自定义功能块'} 副本`
  })
  if (saved) {
    editCustomComponent(saved, { silent: true })
    saveMessage.value = `已复制「${component.name}」`
  } else {
    saveMessage.value = '复制失败'
  }
  window.setTimeout(() => { saveMessage.value = '' }, 2200)
}

function editCustomComponent(component, options = {}) {
  if (!component) return
  const cloned = JSON.parse(JSON.stringify(component))
  draft.id = cloned.id || null
  draft.name = cloned.name || '自定义功能块'
  draft.width = Math.max(1, Number(cloned.width) || 180)
  draft.height = Math.max(1, Number(cloned.height) || 120)
  draft.cropX = 0
  draft.cropY = 0
  draft.cropWidth = draft.width
  draft.cropHeight = draft.height
  draft.bindVariable = Boolean(cloned.bindVariable)
  draft.statusOnColor = cloned.statusOnColor || '#22c55e'
  draft.statusOffColor = cloned.statusOffColor || '#ef4444'
  draft.shapes = Array.isArray(cloned.shapes) && cloned.shapes.length ? cloned.shapes : [defaultShape()]
  draft.ports = Array.isArray(cloned.ports) ? cloned.ports.map(port => {
    const position = normalizePortPosition(port)
    return {
      id: port.id || makeId('port'),
      side: ['top', 'right', 'bottom', 'left'].includes(port.side) ? port.side : 'right',
      x: position.x,
      y: position.y
    }
  }) : []
  selectedShapeId.value = draft.shapes[0]?.id || null
  if (options.silent) return
  saveMessage.value = `正在编辑「${draft.name}」`
  window.setTimeout(() => { saveMessage.value = '' }, 1800)
}

function handleDesignerKeydown(event) {
  const targetTag = event.target?.tagName
  if (['INPUT', 'TEXTAREA', 'SELECT'].includes(targetTag)) return
  if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === 'c') {
    copySelectedShape()
    event.preventDefault()
    return
  }
  if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === 'v') {
    pasteShape()
    event.preventDefault()
  }
}

onMounted(() => {
  document.addEventListener('keydown', handleDesignerKeydown)
})

onUnmounted(() => {
  document.removeEventListener('keydown', handleDesignerKeydown)
})

function saveComponent() {
  const wasEditing = Boolean(draft.id)
  const payload = componentPayloadFromDraft()
  const saved = store.saveCustomComponent({
    id: draft.id || makeId('custom'),
    name: draft.name,
    width: payload.width,
    height: payload.height,
    bindVariable: draft.bindVariable,
    statusOnColor: draft.statusOnColor,
    statusOffColor: draft.statusOffColor,
    shapes: payload.shapes,
    ports: payload.ports
  })
  if (saved) {
    editCustomComponent(saved, { silent: true })
    saveMessage.value = wasEditing ? `已保存「${saved.name}」` : `已添加「${saved.name}」`
  } else {
    saveMessage.value = '保存失败'
  }
  window.setTimeout(() => { saveMessage.value = '' }, 2200)
}

function triggerImport() {
  importFileInput.value?.click()
}

function exportComponentLibrary() {
  api.exportCustomComponents()
    .then(r => {
      const blob = new Blob([r.data], { type: 'application/json' })
      const url = URL.createObjectURL(blob)
      const link = document.createElement('a')
      link.href = url
      link.download = 'custom-components.json'
      document.body.appendChild(link)
      link.click()
      link.remove()
      URL.revokeObjectURL(url)
      saveMessage.value = '组件库已导出'
    })
    .catch(() => {
      saveMessage.value = '导出失败：后端不可用或组件库为空'
    })
    .finally(() => {
      window.setTimeout(() => { saveMessage.value = '' }, 2400)
    })
}

function importComponentLibrary(event) {
  const file = event.target.files?.[0]
  if (!file) return
  const reader = new FileReader()
  reader.onload = async () => {
    try {
      const payload = JSON.parse(String(reader.result))
      await store.importCustomComponentsFromServer(payload)
      saveMessage.value = '组件导入成功'
    } catch {
      saveMessage.value = '导入失败：请确认是有效的组件 JSON'
    }
    window.setTimeout(() => { saveMessage.value = '' }, 2600)
    event.target.value = ''
  }
  reader.readAsText(file, 'utf-8')
}
</script>

<style scoped>
.designer-overlay {
  width: 100%;
  height: 100%;
  display: flex;
  align-items: stretch;
  justify-content: stretch;
  background: var(--bg-secondary);
}
.designer-dialog {
  width: 100%;
  height: 100%;
  display: flex;
  flex-direction: column;
  overflow: hidden;
  background: var(--bg-primary);
  border: 0;
  border-radius: 0;
  box-shadow: none;
}
.designer-header {
  display: flex;
  align-items: flex-start;
  justify-content: space-between;
  gap: 12px;
  padding: 18px 20px 14px;
  border-bottom: 1px solid var(--border-light);
  background: var(--gradient-pane);
}
.designer-header h3 {
  margin: 0;
  font-size: 18px;
  color: var(--text-primary);
}
.designer-header p {
  margin: 4px 0 0;
  color: var(--text-muted);
  font-size: var(--fs-xs);
}
.designer-body {
  flex: 1;
  min-height: 0;
  display: grid;
  grid-template-columns: var(--designer-left-panel-width, 300px) 8px minmax(360px, 1fr) 8px var(--designer-right-panel-width, 320px);
}
.designer-panel {
  min-height: 0;
  overflow-y: auto;
  padding: 16px;
  background: #ffffff;
}
.designer-panel-left,
.designer-panel-right,
.designer-panel:last-child {
  border: 0;
}
.panel-resizer {
  position: relative;
  z-index: 4;
  width: 8px;
  min-width: 8px;
  background: linear-gradient(90deg, #eef2f7 0, #d9e2ef 50%, #eef2f7 100%);
  cursor: col-resize;
}
.panel-resizer::before {
  content: '';
  position: absolute;
  top: 50%;
  left: 50%;
  width: 2px;
  height: 38px;
  border-radius: 999px;
  background: #94a3b8;
  transform: translate(-50%, -50%);
  opacity: 0.7;
}
.panel-resizer:hover,
.panel-resizer:active {
  background: #dbeafe;
}
.panel-resizer:hover::before,
.panel-resizer:active::before {
  background: #2563eb;
  opacity: 1;
}
:global(body.resizing-designer-panel) {
  cursor: col-resize;
  user-select: none;
}
.shape-tool-groups {
  display: flex;
  flex-direction: column;
  gap: 10px;
  margin: 10px 0 14px;
}
.shape-tool-group {
  padding: 9px;
  border: 1px solid var(--border-light);
  border-radius: var(--radius-md);
  background: var(--surface-muted);
}
.shape-tool-title {
  margin-bottom: 7px;
  color: var(--text-secondary);
  font-size: var(--fs-xs);
  font-weight: 800;
}
.designer-tool-grid {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 7px;
}
.shape-tool-btn {
  display: flex;
  align-items: center;
  justify-content: flex-start;
  gap: 6px;
  min-width: 0;
  padding: 5px 7px;
}
.shape-tool-icon {
  width: 16px;
  flex: 0 0 16px;
  color: var(--primary-color);
  font-weight: 900;
  text-align: center;
}
.boundary-section {
  margin: 4px 0 12px;
}
.designer-save {
  width: 100%;
}
.designer-save-row {
  display: grid;
  grid-template-columns: minmax(0, 1fr) 58px;
  align-items: center;
  gap: 8px;
}
.designer-note {
  margin-top: 10px;
  padding: 8px 10px;
  border-radius: var(--radius-sm);
  background: var(--success-bg);
  color: var(--success-text);
  font-size: var(--fs-xs);
  font-weight: 700;
}
.designer-stage-wrap {
  min-width: 0;
  min-height: 0;
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 24px;
  background: #eef2f7;
  overflow: auto;
}
.designer-stage {
  width: 100%;
  height: 100%;
  max-height: 620px;
  background: #ffffff;
  border: 1px solid #cbd5e1;
  border-radius: 8px;
  box-shadow: inset 0 0 0 1px rgba(255, 255, 255, 0.55);
  overflow: visible;
}
.designer-selection {
  fill: none;
  stroke: #2563eb;
  stroke-width: 1.5;
  stroke-dasharray: 6 5;
  pointer-events: none;
}
.designer-resize-handle {
  fill: #2563eb;
  stroke: #ffffff;
  stroke-width: 1.5;
  cursor: nwse-resize;
  vector-effect: non-scaling-stroke;
}
.designer-rotate-arm {
  stroke: #2563eb;
  stroke-width: 1.4;
  stroke-dasharray: 4 3;
  pointer-events: none;
  vector-effect: non-scaling-stroke;
}
.designer-rotate-handle {
  fill: #ffffff;
  stroke: #2563eb;
  stroke-width: 2;
  cursor: grab;
  vector-effect: non-scaling-stroke;
}
.designer-rotate-handle:active {
  cursor: grabbing;
}
.full-width {
  width: 100%;
}
.designer-port {
  fill: #0ea5e9;
  stroke: #ffffff;
  stroke-width: 2;
  cursor: grab;
  vector-effect: non-scaling-stroke;
}
.designer-port:active {
  cursor: grabbing;
}
.designer-boundary {
  fill: rgba(245, 158, 11, 0.06);
  stroke: #f59e0b;
  stroke-width: 2;
  stroke-dasharray: 8 5;
  pointer-events: none;
  vector-effect: non-scaling-stroke;
}
.config-section {
  padding: 10px 0;
  border-top: 1px solid var(--border-light);
}
.section-title {
  margin: 2px 0 8px;
  color: var(--text-primary);
  font-size: var(--fs-xs);
  font-weight: 800;
}
.inline-check {
  display: flex;
  align-items: center;
  gap: 7px;
  min-height: 30px;
  color: var(--text-secondary);
  font-size: var(--fs-xs);
  font-weight: 700;
}
.designer-empty {
  padding: 12px;
  border: 1px dashed var(--border-color);
  border-radius: var(--radius-md);
  color: var(--text-placeholder);
  font-size: var(--fs-xs);
  text-align: center;
}
.designer-empty.compact {
  padding: 9px;
}
.designer-action-row {
  display: grid;
  grid-template-columns: repeat(3, minmax(0, 1fr));
  gap: 7px;
}
.designer-action-row .btn {
  min-width: 0;
}
.saved-component-section {
  margin-top: 14px;
}
.saved-component-header {
  display: flex;
  align-items: flex-start;
  justify-content: space-between;
  gap: 8px;
  margin-bottom: 8px;
}
.saved-component-count {
  color: var(--text-tertiary);
  font-size: 11px;
  font-weight: 700;
}
.saved-component-header-actions {
  display: flex;
  align-items: center;
  gap: 6px;
  flex-shrink: 0;
}
.hidden-file-input {
  display: none;
}
.saved-component-tools {
  display: grid;
  grid-template-columns: minmax(0, 1fr) 104px;
  gap: 7px;
  margin-bottom: 8px;
}
.saved-component-tools input,
.saved-component-tools select {
  min-width: 0;
  height: 30px;
  font-size: var(--fs-xs);
}
.saved-component-list {
  display: flex;
  flex-direction: column;
  gap: 7px;
  max-height: min(390px, 42vh);
  overflow-y: auto;
  padding-right: 3px;
}
.saved-component-row {
  display: grid;
  grid-template-columns: minmax(0, 1fr) auto;
  align-items: center;
  gap: 8px;
  min-height: 44px;
  padding: 7px;
  border: 1px solid var(--border-light);
  border-radius: var(--radius-sm);
  background: var(--surface-muted);
}
.saved-component-row.active {
  border-color: #4f6fb8;
  background: #eef4ff;
}
.saved-component-meta {
  min-width: 0;
  display: flex;
  flex-direction: column;
  gap: 2px;
}
.saved-component-name {
  min-width: 0;
  overflow: hidden;
  color: var(--text-secondary);
  font-size: var(--fs-xs);
  font-weight: 800;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.saved-component-meta small {
  overflow: hidden;
  color: var(--text-tertiary);
  font-size: 11px;
  font-weight: 700;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.saved-component-actions {
  display: flex;
  align-items: center;
  gap: 5px;
}
.saved-component-actions .btn {
  min-width: 40px;
  padding: 4px 7px;
}
.designer-port-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 8px;
  margin-top: 16px;
}
.designer-port-list {
  display: flex;
  flex-direction: column;
  gap: 8px;
}
.designer-port-row {
  display: grid;
  grid-template-columns: 58px minmax(0, 1fr) minmax(0, 1fr) 28px;
  align-items: center;
  gap: 6px;
  padding: 7px;
  border: 1px solid var(--border-light);
  border-radius: var(--radius-sm);
  background: var(--surface-muted);
}
.designer-port-row select,
.designer-port-row input {
  min-width: 0;
}
.port-percent-field {
  display: grid;
  grid-template-columns: 15px minmax(0, 1fr);
  align-items: center;
  gap: 4px;
  color: var(--text-tertiary);
  font-size: 11px;
  font-weight: 800;
}
.port-percent-field input {
  height: 30px;
  padding: 0 5px;
  text-align: center;
}

/* Keep the component designer readable even when global compact styles change. */
.designer-dialog {
  font-size: 14px;
}
.designer-dialog label,
.designer-dialog input,
.designer-dialog select,
.designer-dialog button,
.designer-dialog .btn,
.designer-dialog .btn-sm {
  font-size: 13px;
}
.designer-header p,
.shape-tool-title,
.section-title,
.inline-check,
.designer-note,
.designer-empty,
.saved-component-tools input,
.saved-component-tools select,
.saved-component-name,
.saved-component-actions .btn,
.port-percent-field {
  font-size: 13px;
}
.saved-component-count,
.saved-component-meta small {
  font-size: 12px;
}
.shape-tool-icon {
  font-size: 14px;
  line-height: 1;
}
@media (max-width: 980px) {
  .designer-body {
    grid-template-columns: 1fr;
    overflow-y: auto;
  }
  .panel-resizer {
    display: none;
  }
  .designer-panel,
  .designer-panel:last-child {
    border: 0;
    border-bottom: 1px solid var(--border-light);
  }
  .designer-stage-wrap {
    min-height: 360px;
  }
}
</style>
