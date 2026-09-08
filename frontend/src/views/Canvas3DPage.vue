<template>
  <section class="scene-page" :class="[{ 'is-running': applicationMode }, `theme-${scene.environment.theme}`]">
    <input ref="importInput" type="file" accept=".json,application/json" hidden @change="importScene" />
    <div ref="workspaceHost" class="scene-workspace">
      <aside v-if="!applicationMode" ref="libraryAside" class="scene-library" :style="{ width: `${leftWidth}px`, flexBasis: `${leftWidth}px` }">
        <section class="library-group component-library-group">
        <div class="library-header">
          <div class="library-header-row">
            <h3>组件库</h3>
            <button class="library-collapse" type="button" :aria-expanded="libraryOpen" :aria-label="`${libraryOpen ? '折叠' : '展开'}工业组件库`" @click="libraryOpen = !libraryOpen; hideModelPreview()">{{ libraryOpen ? '▾' : '▸' }}</button>
          </div>
          <p>按行业场景和公共组件组织</p>
        </div>
        <div v-show="libraryOpen" class="library-search">
          <input v-model="librarySearch" type="search" placeholder="搜索组件、分类或场景" @keydown.stop @keyup.stop />
          <button v-if="librarySearch" type="button" title="清空搜索" @click="librarySearch = ''">清除</button>
        </div>
        <div v-show="libraryOpen" class="library-section-body">
        <div class="model-library">
          <button v-for="model in filteredModelLibrary" :key="model.type" class="model-card" draggable="true" @mouseenter="showModelPreview(model, $event)" @mouseleave="hideModelPreview" @focus="showModelPreview(model, $event)" @blur="hideModelPreview" @dragstart="hideModelPreview(); dragModel($event, model.type)" @click="addModel(model.type)">
            <span class="model-icon" :style="{ color: model.color }">{{ model.icon }}</span><span>{{ model.name }}</span>
          </button>
        </div>
        <div v-if="!filteredModelLibrary.length" class="muted-empty component-empty">没有匹配的组件</div>
        </div>
        </section>
        <section class="library-group device-library-group">
        <button class="panel-heading tree-heading section-toggle" :aria-expanded="devicesOpen" :aria-label="`${devicesOpen ? '折叠' : '展开'}场景设备`" @click="devicesOpen = !devicesOpen"><span class="heading-title"><i>{{ devicesOpen ? '▾' : '▸' }}</i><strong>场景设备</strong></span><span class="heading-meta">{{ scene.nodes.length }}</span></button>
        <div v-show="devicesOpen" class="scene-devices-body">
        <input v-model="search" class="scene-search" placeholder="搜索设备名称或编号" aria-label="搜索场景设备" />
        <div class="scene-tree">
          <div v-if="!filteredNodes.length" class="muted-empty">{{ scene.nodes.length ? '没有匹配的设备' : '从组件库添加第一台设备' }}</div>
          <div v-for="node in filteredNodes" :key="node.id" class="tree-row" :class="{ selected: selectedId === node.id, hidden: !node.visible }">
            <button class="tree-select" @click="selectNode(node.id)" @dblclick="engine?.focus(node.id)"><span class="state-dot" :data-state="runtime(node).state"></span><span>{{ node.name }}</span></button>
            <button :aria-label="`${node.visible ? '隐藏' : '显示'}${node.name}`" :title="node.visible ? '隐藏设备' : '显示设备'" @click="patchNode(node.id, { visible: !node.visible })">{{ node.visible ? '◉' : '○' }}</button>
            <button :aria-label="`${node.locked ? '解锁' : '锁定'}${node.name}`" :title="node.locked ? '解锁设备' : '锁定设备'" @click="patchNode(node.id, { locked: !node.locked })">{{ node.locked ? '▣' : '▫' }}</button>
          </div>
        </div>
        </div>
        </section>
      </aside>
      <PaneResizeHandle v-if="!applicationMode" v-model="leftWidth" :min="140" :max="maxLeftWidth" label="拖动调整 3D 左侧组件栏宽度" @resize-start="engine?.setResizing(true)" @resize-end="engine?.setResizing(false)" />
      <SceneModelPreview v-if="!applicationMode && previewModel" :model="previewModel" :style="{ left: `${leftWidth + 12}px`, top: `${previewTop}px` }" />

      <main ref="centerHost" class="scene-center">
        <div v-if="!applicationMode" class="scene-toolbar canvas-action-toolbar">
          <button class="btn btn-sm" aria-label="项目" title="项目管理" @click="projectDialog = true"><span class="toolbar-action-icon scene-tool-icon">📁</span>项目</button>
          <button class="btn btn-sm" aria-label="保存" title="保存当前 2D 画布和 3D 场景（Ctrl+S）" @click="saveProject"><span class="toolbar-action-icon scene-tool-icon">💾</span>保存</button>
          <button class="btn btn-sm" aria-label="导入 3D JSON" title="从 JSON 文件导入 3D 场景" @click="importInput?.click()"><span class="toolbar-action-icon scene-tool-icon">📥</span>导入 3D JSON</button>
          <button class="btn btn-sm" aria-label="导出 3D JSON" title="将当前 3D 场景导出为 JSON 文件" @click="exportScene"><span class="toolbar-action-icon scene-tool-icon">📤</span>导出 3D JSON</button>
          <button class="btn btn-sm btn-primary" aria-label="应用模式" title="只显示监控画面；Esc 返回编辑，F2 切换 2D" @click="applicationMode = true"><span class="toolbar-action-icon scene-tool-icon">▶</span>应用模式</button>
        </div>
        <div class="scene-stage" @dragover.prevent @drop.prevent="dropModel">
          <div v-if="notice && !applicationMode" class="scene-notice" role="status"><span>{{ notice }}</span><button aria-label="关闭提示" @click="clearNotice">×</button></div>
          <div ref="stageHost" class="scene-render-host"></div>
          <div v-if="!applicationMode" class="viewport-toolbar">
            <div v-if="!applicationMode" class="viewport-tool-group">
              <button v-for="tool in transformTools" :key="tool.key" :class="{ active: transformMode === tool.key }" @click="transformMode = tool.key">{{ tool.name }}</button>
              <button :class="{ active: snap }" @click="snap = !snap" title="移动 0.5m / 旋转 15° 吸附">吸附</button>
            </div>
            <div class="viewport-tool-group">
              <button @click="engine?.focus()" title="显示全部设备">总览</button>
              <button @click="engine?.focus(null, 'top')">俯视</button>
              <button @click="engine?.focus(null, 'front')">正视</button>
              <button @click="engine?.focus(null, 'side')">侧视</button>
            </div>
          </div>
          <div v-if="!applicationMode" class="scene-history">
            <button :disabled="!store.canUndoScene" title="撤销 Ctrl+Z" @click="store.undoScene">↶</button><button :disabled="!store.canRedoScene" title="重做 Ctrl+Y" @click="store.redoScene">↷</button>
          </div>
          <div v-if="renderError" class="scene-empty scene-render-error" role="alert"><strong>3D 画面暂不可用</strong><p>{{ renderError }}</p><button class="btn btn-primary" @click="store.canvasViewMode = '2d'">返回 2D 监控</button></div>
          <div v-else-if="!scene.nodes.length" class="scene-empty"><span class="empty-cube">◇</span><strong>构建你的工业现场</strong><p>从左侧组件库点击或拖入设备，绑定已有变量，即可开始三维监控。</p></div>
          <div class="scene-compass"><b>Y</b><span>Z ── X</span></div>
          <div class="scene-legend"><span><i data-state="running"></i>运行 / 正常</span><span><i data-state="fault"></i>故障</span><span><i data-state="offline"></i>离线</span></div>
          <div v-if="!applicationMode" class="scene-navigation-hint">左键旋转 · 按住滚轮 / 右键平移 · 滚轮滚动缩放</div>
        </div>
        <PaneResizeHandle v-if="!applicationMode" v-model="dockHeight" axis="y" reverse :min="70" :max="maxDockHeight" label="拖动调整 3D 下侧数据栏高度" @resize-start="engine?.setResizing(true)" @resize-end="engine?.setResizing(false)" />
        <div v-if="!applicationMode" class="scene-data-dock" :style="{ height: `${dockHeight}px` }">
          <div class="data-tabs"><button :class="{ active: dataTab === 'devices' }" @click="dataTab = 'devices'">设备状态 <span>{{ monitoredNodes.length }}</span></button><button :class="{ active: dataTab === 'alarms' }" @click="dataTab = 'alarms'">共用报警 <span>{{ store.alarms.length }}</span></button><button :class="{ active: dataTab === 'trend' }" @click="dataTab = 'trend'">选中设备趋势</button><span class="shared-data-note">现有采集数据 · 自动刷新</span></div>
          <div v-if="dataTab === 'devices'" class="device-status-list">
            <button v-for="item in deviceStatusItems" :key="item.node.id" class="device-status-card" :class="{ selected: item.node.id === selectedId }" :data-card-state="item.live.state" @click="selectNode(item.node.id); engine?.focus(item.node.id)">
              <span class="device-status-name"><i class="state-dot" :data-state="item.live.state"></i><b>{{ item.node.name }}</b></span>
              <strong>{{ item.live.text }}</strong>
              <span class="device-status-meta">{{ item.node.deviceId || modelName(item.node.type) }}</span>
              <small>{{ item.live.label }}</small>
            </button>
            <div v-if="!deviceStatusItems.length" class="muted-empty">添加设备后，在属性面板绑定现有变量。</div>
          </div>
          <div v-else-if="dataTab === 'alarms'" class="scene-alarm-list"><button v-for="alarm in store.alarms.slice(0, 50)" :key="alarm.id" @click="locateAlarm(alarm)"><span class="state-dot" data-state="fault"></span><strong>{{ alarm.varName }}</strong><span>{{ alarm.message || alarm.alarmType }}</span><small>{{ alarm.acknowledged ? '已确认' : '未确认' }}</small></button><div v-if="!store.alarms.length" class="muted-empty">暂无报警记录</div></div>
          <div v-else class="scene-trend"><TrendChartWidget v-if="trendIds.length" :key="selectedId" :widget="trendWidget" /><div v-else class="muted-empty">选中已绑定变量的设备以查看共用趋势。</div></div>
        </div>
      </main>

      <PaneResizeHandle v-if="!applicationMode" v-model="rightWidth" reverse :min="160" :max="maxRightWidth" label="拖动调整 3D 右侧属性栏宽度" @resize-start="engine?.setResizing(true)" @resize-end="engine?.setResizing(false)" />
      <aside v-if="!applicationMode" class="scene-inspector" :style="{ width: `${rightWidth}px`, flexBasis: `${rightWidth}px` }">
        <div class="panel-heading"><strong>{{ selected ? (applicationMode ? '设备详情' : '设备属性') : '场景设置' }}</strong><button v-if="selected" class="inspector-close" aria-label="关闭设备属性" @click="selectNode(null)">×</button><span v-else>SCENE</span></div>
        <div class="inspector-scroll">
          <template v-if="selected">
            <div class="device-summary"><span class="summary-type">{{ modelName(selected.type) }}</span><h3>{{ selected.name }}</h3><div><i class="state-dot" :data-state="selectedRuntime.state"></i>{{ selectedRuntime.label }}</div><strong>{{ selectedRuntime.text }}</strong><small>{{ selected.deviceId || '尚未设置设备编号' }}</small></div>
            <button class="inspector-focus" @click="engine?.focus(selected.id)">◎ 定位到设备</button>
            <template v-if="!applicationMode">
              <label class="inspector-field">组件名称<input ref="sceneNameInput" v-model="selectedNameDraft" maxlength="100" @change="commitSelectedName" @keydown.enter="$event.currentTarget.blur()" /></label>
              <label class="inspector-field">名称字号（px）<input type="number" min="8" max="48" step="1" :value="selected.labelFontSize ?? scene.environment.labelFontSize" @change="editField('labelFontSize', Number($event.target.value))" /></label>
              <button v-if="selected.labelFontSize != null" class="label-size-reset" @click="editField('labelFontSize', null)">使用场景统一字号</button>
              <label class="inspector-field">设备编号<input :value="selected.deviceId" maxlength="100" placeholder="例如 P-101" @change="editField('deviceId', $event.target.value)" /></label>
              <div class="inspector-section-title">空间变换 <span>位置：m / 旋转：°</span></div>
              <label v-for="field in vectorFields" :key="field.key" class="inspector-field">{{ field.name }}<div class="vector-inputs"><label v-for="(axis, index) in ['X', 'Y', 'Z']" :key="axis"><span :class="`axis-${axis}`">{{ axis }}</span><input type="number" :aria-label="`${field.name} ${axis}`" :value="Number(selected[field.key][index].toFixed(2))" :disabled="selected.locked" :step="field.key === 'rotation' ? 15 : 0.1" :min="field.key === 'scale' ? 0.05 : -10000" :max="field.key === 'scale' ? 100 : 10000" @change="editVector(field.key, index, $event.target.value)" /></label></div></label>
              <label class="inspector-field color-field">设备颜色<input type="color" :value="selected.color" :disabled="selected.locked" @change="editField('color', $event.target.value)" /></label>
              <label v-if="selected.type === 'pipe'" class="inspector-field">管道长度（m）<input type="number" min="0.2" max="200" step="0.5" :value="selected.pipeLength" :disabled="selected.locked" @change="editField('pipeLength', Number($event.target.value))" /></label>
              <div class="inspector-section-title">共用数据绑定</div>
              <label class="inspector-field">跟随 2D 组件<select :value="selected.linkedWidgetId || ''" @change="editField('linkedWidgetId', $event.target.value || null)"><option value="">独立选择已有变量</option><option v-if="selected.linkedWidgetId && !linkedWidgets.some(w => String(w.id) === selected.linkedWidgetId)" :value="selected.linkedWidgetId">原 2D 组件已不存在</option><option v-for="widget in linkedWidgets" :key="widget.id" :value="widget.id">{{ widget.config?.label || widget.type }} · {{ widget.id }}</option></select></label>
              <p v-if="selected.linkedWidgetId" class="field-note">{{ selected.type === 'tank' ? '液位数值' : '运行状态' }}跟随该 2D 组件的变量绑定。</p>
              <input v-model="variableSearch" class="scene-search binding-search" placeholder="搜索已有采集变量" aria-label="搜索绑定变量" />
              <label v-for="binding in bindingFields" :key="binding.key" class="inspector-field">{{ binding.name }}<select :value="bindings[binding.key] || ''" :disabled="!!selected.linkedWidgetId && binding.key === (selected.type === 'tank' ? 'value' : 'status')" @change="editBinding(binding.key, $event.target.value)"><option value="">未绑定</option><option v-if="bindings[binding.key] && !filteredVariables.some(v => String(v.id) === String(bindings[binding.key]))" :value="bindings[binding.key]">{{ bindingLabel(bindings[binding.key]) }}</option><option v-for="variable in filteredVariables" :key="variable.id" :value="String(variable.id)">{{ variable.name }}{{ variable.unit ? ` (${variable.unit})` : '' }} · {{ variable.id }}</option></select></label>
              <p class="field-note">运行值大于 0 表示运行；故障值非 0 表示故障。动画仅由有效实时数据驱动。</p>
              <div class="range-fields"><label class="inspector-field">量程下限<input type="number" :value="selected.range.min" @change="editRange('min', $event.target.value)" /></label><label class="inspector-field">量程上限<input type="number" :value="selected.range.max" @change="editRange('max', $event.target.value)" /></label></div>
              <label class="inspector-field">数据过期时间（秒）<input type="number" min="1" max="3600" :value="selected.staleAfterMs / 1000" @change="editField('staleAfterMs', Number($event.target.value) * 1000)" /></label>
              <div class="inspector-node-actions"><button class="btn btn-sm" @click="duplicateSelected">复制设备</button><button class="btn btn-sm" @click="editField('locked', !selected.locked)">{{ selected.locked ? '解锁' : '锁定' }}</button><button class="btn btn-sm btn-danger" :disabled="selected.locked" @click="deleteSelected">删除</button></div>
            </template>
            <template v-else><div class="inspector-section-title">绑定点位</div><div v-for="binding in bindingFields" :key="binding.key" class="runtime-binding"><span>{{ binding.name }}</span><strong>{{ bindings[binding.key] ? bindingLabel(bindings[binding.key]) : '未绑定' }}</strong></div><button class="inspector-focus" @click="router.push('/parameters')">打开共用参数下发</button><p class="field-note">参数操作使用现有通信资源与下发流程。</p></template>
          </template>
          <template v-else>
            <label class="inspector-field">画面主题<select :value="scene.environment.theme" @change="setEnvironment('theme', $event.target.value)"><option value="light">工业浅色</option><option value="dark">深色监控</option></select></label>
            <label class="inspector-field">渲染质量<select :value="scene.environment.quality" @change="setEnvironment('quality', $event.target.value)"><option value="smooth">流畅 · 低功耗</option><option value="standard">标准 · 推荐</option><option value="fine">精细 · 高清</option></select></label>
            <label class="inspector-check"><input type="checkbox" :checked="scene.environment.grid" @change="setEnvironment('grid', $event.target.checked)" />显示地面网格</label>
            <label class="inspector-check"><input type="checkbox" :checked="scene.environment.labels" @change="setEnvironment('labels', $event.target.checked)" />显示设备标签</label>
            <label class="inspector-field">设备名称统一字号（px）<input type="number" min="8" max="48" step="1" :value="scene.environment.labelFontSize" @change="setEnvironment('labelFontSize', Number($event.target.value))" /></label>
            <div class="scene-counts"><div><strong>{{ scene.nodes.length }}</strong><span>场景组件</span></div><div><strong>{{ boundCount }}</strong><span>已绑定设备</span></div></div>
            <p class="field-note">场景坐标以米为单位。保存项目时，2D 布局与 3D 场景一并保存；单独导出 3D JSON 可用于场景迁移。</p>
            <button v-if="!applicationMode && scene.nodes.length" class="btn btn-sm btn-danger clear-scene" @click="confirmAction = { title: '清空 3D 场景', message: `将移除 ${scene.nodes.length} 个 3D 组件，2D 画布和采集资源保持不变。可通过撤销恢复。`, action: () => store.replaceScene(emptyScene()) }">清空 3D 场景</button>
          </template>
        </div>
      </aside>
    </div>
    <ProjectManagerDialog v-if="projectDialog" :current-layout="store.layoutSnapshot" :open-create-on-mount="createProjectOnOpen" @close="projectDialog = false; createProjectOnOpen = false" @load-layout="projectLoaded" />
    <ConfirmDialog v-if="confirmAction" :title="confirmAction.title" :message="confirmAction.message" confirm-text="继续" @close="confirmAction = null" @confirm="runConfirmed" />
  </section>
</template>

<script setup>
import { ref, computed, watch, onMounted, onBeforeUnmount, nextTick } from 'vue'
import { useRouter } from 'vue-router'
import { useMonitorStore } from '../store/index.js'
import { useProjectStore } from '../store/projectStore.js'
import ProjectManagerDialog from '../components/projects/ProjectManagerDialog.vue'
import ConfirmDialog from '../components/common/ConfirmDialog.vue'
import PaneResizeHandle from '../components/common/PaneResizeHandle.vue'
import SceneModelPreview from '../components/scene3d/SceneModelPreview.vue'
import { usePanelSize } from '../composables/usePanelSize.js'
import TrendChartWidget from '../components/widgets/TrendChartWidget.vue'
import { createSceneRenderer } from '../services/scene3dRenderer.js'
import { MODEL_LIBRARY, createSceneNode, emptyScene, copyScene, sceneId, effectiveBindings, nodeRuntime, sceneExport, parseSceneFile, downloadJson } from '../services/scene3dService.js'

const store = useMonitorStore(), projects = useProjectStore(), router = useRouter()
const scene = computed(() => store.scene3d)
const applicationMode = computed({ get: () => store.canvasApplicationMode, set: value => { store.canvasApplicationMode = value } })
const stageHost = ref(null), importInput = ref(null), sceneNameInput = ref(null), selectedId = ref(null), selectedNameDraft = ref(''), librarySearch = ref(''), search = ref(''), variableSearch = ref(''), notice = ref(''), renderError = ref('')
const workspaceHost = ref(null), centerHost = ref(null), libraryAside = ref(null)
const libraryOpen = ref(true), devicesOpen = ref(true), previewModel = ref(null), previewTop = ref(8)
const leftWidth = usePanelSize('monitoring.scene3d.leftWidth', 224)
const rightWidth = usePanelSize('monitoring.scene3d.rightWidth', 264)
const dockHeight = usePanelSize('monitoring.scene3d.dockHeight', 172)
const maxLeftWidth = () => Math.max(140, (workspaceHost.value?.clientWidth || window.innerWidth) - rightWidth.value - 174)
const maxRightWidth = () => Math.max(160, (workspaceHost.value?.clientWidth || window.innerWidth) - leftWidth.value - 174)
const maxDockHeight = () => Math.max(70, (centerHost.value?.clientHeight || window.innerHeight) - (centerHost.value?.querySelector('.scene-toolbar')?.offsetHeight || 46) - 167)
function fitPanels() {
  if (applicationMode.value) return
  leftWidth.value = Math.max(140, Math.min(leftWidth.value, maxLeftWidth()))
  rightWidth.value = Math.max(160, Math.min(rightWidth.value, maxRightWidth()))
  dockHeight.value = Math.max(70, Math.min(dockHeight.value, maxDockHeight()))
}
const projectDialog = ref(false), createProjectOnOpen = ref(false), confirmAction = ref(null)
const transformMode = ref('translate'), snap = ref(true), dataTab = ref('devices')
const transformTools = [{ key: 'translate', name: '移动' }, { key: 'rotate', name: '旋转' }, { key: 'scale', name: '缩放' }]
const vectorFields = [{ key: 'position', name: '位置' }, { key: 'rotation', name: '旋转' }, { key: 'scale', name: '缩放' }]
const bindingFields = [{ key: 'status', name: '运行状态变量' }, { key: 'value', name: '数值 / 液位变量' }, { key: 'fault', name: '故障状态变量' }]
let engine = null, copiedNode = null, pasteSequence = 0, noticeTimer = null
function clearNotice() {
  if (noticeTimer) clearTimeout(noticeTimer)
  noticeTimer = null
  notice.value = ''
}
function showNotice(message) {
  clearNotice()
  notice.value = message
  noticeTimer = setTimeout(clearNotice, 3000)
}
const selected = computed(() => scene.value.nodes.find(node => node.id === selectedId.value))
const linkedWidgets = computed(() => store.widgets.filter(widget => widget.config && Object.hasOwn(widget.config, 'varId')))
const bindings = computed(() => selected.value ? effectiveBindings(selected.value, store.widgets) : {})
const filteredModelLibrary = computed(() => {
  const keyword = librarySearch.value.trim().toLowerCase()
  if (!keyword) return MODEL_LIBRARY
  return MODEL_LIBRARY.filter(model => `${model.name} ${model.type} ${model.category || ''}`.toLowerCase().includes(keyword))
})
const filteredNodes = computed(() => scene.value.nodes.filter(node => `${node.name} ${node.deviceId}`.toLowerCase().includes(search.value.toLowerCase())))
const filteredVariables = computed(() => store.variables.filter(v => `${v.name} ${v.id}`.toLowerCase().includes(variableSearch.value.toLowerCase())))
const monitoredNodes = computed(() => scene.value.nodes.filter(node => !['platform', 'building'].includes(node.type)))
const deviceStatusItems = computed(() => {
  const priority = { fault: 0, offline: 1, stale: 2, missing: 3, stopped: 4, running: 5, good: 5, unbound: 6 }
  return monitoredNodes.value
    .map((node, index) => ({ node, index, live: runtime(node) }))
    .sort((a, b) => (priority[a.live.state] ?? 9) - (priority[b.live.state] ?? 9) || a.index - b.index)
})
const boundCount = computed(() => monitoredNodes.value.filter(node => Object.values(effectiveBindings(node, store.widgets)).some(Boolean)).length)
const runtime = node => nodeRuntime(node, store, store.dataClockMs)
const selectedRuntime = computed(() => selected.value ? runtime(selected.value) : {})
const trendIds = computed(() => [...new Set(Object.values(bindings.value).filter(Boolean))].map(id => store.variables.find(v => String(v.id) === String(id))?.id).filter(id => id != null))
const trendWidget = computed(() => ({ id: 'scene-trend', type: 'trendChart', config: { varIds: trendIds.value, timeWindow: 60000, chartFontSize: 13 } }))
const modelName = type => MODEL_LIBRARY.find(model => model.type === type)?.name || type
const bindingLabel = id => store.variables.find(v => String(v.id) === String(id))?.name || `待重新绑定 (${id})`

function showModelPreview(model, event) {
  const workspace = workspaceHost.value?.getBoundingClientRect(), target = event.currentTarget?.getBoundingClientRect()
  if (!workspace || !target) return
  previewTop.value = Math.max(8, Math.min(target.top - workspace.top - 24, workspace.height - 218))
  previewModel.value = model
}
function hideModelPreview() { previewModel.value = null }

function selectNode(id) {
  selectedId.value = id
  selectedNameDraft.value = scene.value.nodes.find(node => node.id === id)?.name || ''
  engine?.setSelected(id)
}
function commitSelectedName() {
  if (!selected.value) return
  const name = selectedNameDraft.value.trim() || modelName(selected.value.type)
  selectedNameDraft.value = name
  if (name !== selected.value.name) editField('name', name)
}
function patchNode(id, patch) {
  try {
    store.replaceScene({ ...scene.value, nodes: scene.value.nodes.map(node => node.id === id ? { ...node, ...patch } : node) })
  } catch (error) { showNotice(error.message); engine?.setScene(scene.value) }
}
function editField(key, value) { if (selected.value) patchNode(selected.value.id, { [key]: value }) }
function editVector(key, index, value) {
  if (!selected.value || selected.value.locked) return
  const vector = [...selected.value[key]]; vector[index] = Number(value); editField(key, vector)
}
function editBinding(key, value) { editField('bindings', { ...selected.value.bindings, [key]: value || null }) }
function editRange(key, value) { editField('range', { ...selected.value.range, [key]: Number(value) }) }
function setEnvironment(key, value) { store.replaceScene({ ...scene.value, environment: { ...scene.value.environment, [key]: value } }) }
function addModel(type, position) {
  if (applicationMode.value) return
  if (scene.value.nodes.length >= 1000) { showNotice('单个场景最多支持 1000 个组件'); return }
  const count = scene.value.nodes.filter(n => n.type === type).length
  const node = createSceneNode(type, { name: `${modelName(type)} ${count + 1}`, position: position || [(scene.value.nodes.length % 5 - 2) * 3, 0, Math.floor(scene.value.nodes.length / 5) * 3] })
  store.replaceScene({ ...scene.value, nodes: [...scene.value.nodes, node] })
  selectNode(node.id)
}
function dragModel(event, type) { event.dataTransfer.setData('application/x-monitor-3d', type); event.dataTransfer.effectAllowed = 'copy' }
function dropModel(event) {
  const type = event.dataTransfer.getData('application/x-monitor-3d')
  if (MODEL_LIBRARY.some(model => model.type === type)) addModel(type, engine?.groundPosition(event.clientX, event.clientY))
}
function duplicateSelected() {
  if (!selected.value || applicationMode.value) return
  if (scene.value.nodes.length >= 1000) { showNotice('单个场景最多支持 1000 个组件'); return }
  const node = copyScene(selected.value)
  node.id = sceneId(); node.name += ' 副本'; node.position[0] += 2; node.position[2] += 1; node.locked = false
  store.replaceScene({ ...scene.value, nodes: [...scene.value.nodes, node] }); selectNode(node.id)
}
function copySelected() {
  if (!selected.value || applicationMode.value) return false
  copiedNode = copyScene(selected.value)
  pasteSequence = 0
  showNotice(`已复制「${selected.value.name}」，按 Ctrl+V 粘贴`)
  return true
}
function pasteCopied() {
  if (!copiedNode || applicationMode.value) return false
  if (scene.value.nodes.length >= 1000) { showNotice('单个场景最多支持 1000 个组件'); return false }
  pasteSequence += 1
  const node = copyScene(copiedNode)
  node.id = sceneId()
  node.name = `${copiedNode.name} 副本${pasteSequence > 1 ? ` ${pasteSequence}` : ''}`
  node.position = [...node.position]
  node.position[0] += 1.5 * pasteSequence
  node.position[2] += pasteSequence
  node.locked = false
  store.replaceScene({ ...scene.value, nodes: [...scene.value.nodes, node] })
  selectNode(node.id)
  showNotice(`已粘贴「${node.name}」`)
  return true
}
function deleteSelected() {
  if (!selected.value || selected.value.locked || applicationMode.value) return
  store.replaceScene({ ...scene.value, nodes: scene.value.nodes.filter(node => node.id !== selectedId.value) }); selectNode(null)
}
function runConfirmed() { const action = confirmAction.value?.action; confirmAction.value = null; action?.() }
async function saveProject() {
  const current = projects.currentProject
  if (!current) { createProjectOnOpen.value = true; projectDialog.value = true; return }
  try {
    await projects.updateProject(current.id, { name: current.name, description: current.description, layout: store.layoutSnapshot })
    store.markProjectSaved(current.id)
    showNotice(`已保存「${current.name}」的 2D 画布和 3D 场景`)
  } catch (error) { showNotice(error.message || '保存失败，请导出 JSON 备份') }
}
function projectLoaded() {
  selectNode(null); engine?.setScene(scene.value); engine?.restoreCamera(scene.value.camera)
  showNotice('已加载项目（包含本地编辑草稿）')
}
function exportScene() {
  downloadJson(`${projects.currentProject?.name || '监控画布'}-3D`, sceneExport(scene.value, projects.currentProject?.name || '3D 场景', store.variables, store.widgets))
  showNotice('已导出完整 3D 场景 JSON，包含内置模型参数、视角和变量引用。')
}
async function importScene(event) {
  const file = event.target.files?.[0]; event.target.value = ''; if (!file) return
  try {
    if (file.size > 10 * 1024 * 1024) throw new Error('场景 JSON 超过 10 MB，请精简场景后导入')
    const { scene: next, unresolved } = parseSceneFile(JSON.parse(await file.text()), store.variables)
    confirmAction.value = {
      title: '导入 3D 场景',
      message: `文件含 ${next.nodes.length} 个组件，${unresolved} 处变量引用需要重新绑定。导入将替换当前 3D 场景，可撤销；2D 画布保持不变。`,
      action: () => { store.replaceScene(next); selectNode(null); engine?.setScene(next); engine?.restoreCamera(next.camera); showNotice(`已导入 ${next.nodes.length} 个 3D 组件${unresolved ? `，请处理 ${unresolved} 处待绑定变量` : ''}`) }
    }
  } catch (error) { showNotice(`导入失败：${error.message}`) }
}
function locateAlarm(alarm) {
  const node = scene.value.nodes.find(n => Object.values(effectiveBindings(n, store.widgets)).some(id => id != null && String(id) === String(alarm.varId)))
  if (node) { selectNode(node.id); engine?.focus(node.id) } else showNotice('该报警变量尚未关联到当前 3D 场景设备')
}
function keydown(event) {
  if (projectDialog.value || confirmAction.value) return
  if (applicationMode.value && event.key === 'Escape') { event.preventDefault(); applicationMode.value = false; return }
  if (applicationMode.value && event.key === 'F2') { event.preventDefault(); store.canvasViewMode = '2d'; return }
  const input = event.target.closest?.('input,textarea,select,[contenteditable="true"]')
  const key = event.key.toLowerCase(), ctrl = event.ctrlKey || event.metaKey
  if (ctrl && key === 's') { event.preventDefault(); saveProject(); return }
  if (input || applicationMode.value) return
  if (ctrl && key === 'c') { if (copySelected()) event.preventDefault() }
  else if (ctrl && key === 'v') { if (pasteCopied()) event.preventDefault() }
  else if (ctrl && key === 'z') { event.preventDefault(); event.shiftKey ? store.redoScene() : store.undoScene() }
  else if (ctrl && key === 'y') { event.preventDefault(); store.redoScene() }
  else if (ctrl && key === 'd') { event.preventDefault(); duplicateSelected() }
  else if (key === 'delete') deleteSelected()
  else if (key === 'escape') selectNode(null)
}
watch(() => [scene.value.nodes, scene.value.environment], () => {
  if (selectedId.value && !selected.value) selectNode(null)
  engine?.setScene(scene.value)
}, { deep: true })
watch(() => selected.value?.name, name => {
  if (document.activeElement !== sceneNameInput.value) selectedNameDraft.value = name || ''
})
watch(transformMode, mode => engine?.setMode(mode))
watch(snap, value => engine?.setSnap(value))
watch(applicationMode, value => { engine?.setEditable(!value); if (!value) nextTick(fitPanels) })
onMounted(() => {
  fitPanels()
  window.addEventListener('resize', fitPanels)
  try {
    engine = createSceneRenderer(stageHost.value, {
      onSelect: selectNode,
      onTransform: (id, patch) => patchNode(id, patch),
      onCamera: camera => store.setSceneCamera(camera),
      onError: message => { renderError.value = message },
      getRuntime: runtime
    })
    engine.setScene(scene.value); engine.restoreCamera(scene.value.camera); engine.setEditable(!applicationMode.value); engine.setSnap(snap.value)
  } catch (error) { renderError.value = '当前浏览器或显卡未能初始化 WebGL 2。请启用硬件加速后重试。'; console.warn('3D initialization failed:', error) }
  document.addEventListener('keydown', keydown)
})
onBeforeUnmount(() => { clearNotice(); engine?.dispose(); engine = null; document.removeEventListener('keydown', keydown); window.removeEventListener('resize', fitPanels); store.persistLayout() })
</script>

<style scoped>
.scene-page { height: 100%; display: flex; flex-direction: column; overflow: hidden; background: #eef2f6; color: #25394b; font-family: var(--font-family); --scene-border: #dbe3ea; }
.scene-page button,.scene-page input,.scene-page select { font-family: inherit; }
.scene-legend i[data-state="running"] { background: #2aaf83; }.scene-legend i[data-state="fault"] { background: #dd5260; }
.scene-notice { position: absolute; z-index: 12; top: 0; left: 0; right: 0; padding: 8px 20px; display: flex; align-items: center; justify-content: space-between; gap: 16px; background: #edf6faf2; border-bottom: 1px solid #cfdfeb; box-shadow: 0 3px 10px #38566b14; font-size: 12px; color: #3e6883; }
.scene-notice button { border: 0; background: transparent; color: inherit; font-size: 20px; cursor: pointer; }
.scene-workspace { position: relative; min-height: 0; flex: 1; display: flex; overflow: hidden; }
.scene-library { width: 224px; flex: 0 0 224px; box-sizing: border-box; background: #f8fafc; border-right: 1px solid var(--scene-border); display: flex; flex-direction: column; min-height: 0; overflow: hidden; box-shadow: 8px 0 18px #0f172a09; }
.library-group { display: flex; flex-direction: column; min-height: 0; overflow: hidden; background: transparent; }
.component-library-group { flex: 0 1 auto; }
.device-library-group { flex: 1 1 180px; border-top: 1px solid #dfe6ed; }
.library-header { flex-shrink: 0; padding: 20px 20px 16px; border-bottom: 1px solid #e2e8ee; background: #fff; }
.library-header-row { display: flex; align-items: center; justify-content: space-between; gap: 10px; }
.library-header h3 { margin: 0; color: #25394b; font-size: 18px; font-weight: 700; }
.library-header p { margin: 7px 0 0; color: #7f909d; font-size: 13px; line-height: 1.5; }
.library-collapse { width: 28px; height: 28px; flex: 0 0 28px; border: 0; border-radius: 6px; background: transparent; color: #6d8799; font-size: 16px; cursor: pointer; }
.library-collapse:hover { background: #eef4f8; }
.library-search { display: flex; align-items: center; gap: 6px; padding: 10px 14px; background: #fff; border-bottom: 1px solid #e5ebf2; }
.library-search input { flex: 1; min-width: 0; height: 34px; padding: 0 11px; border: 1px solid #d7e0e8; border-radius: 7px; background: #f8fafc; color: #25394b; font-size: 13px; outline: none; transition: border-color .18s, box-shadow .18s, background .18s; }
.library-search input:focus { background: #fff; border-color: #7aa8c3; box-shadow: 0 0 0 3px #7aa8c31f; }
.library-search button { height: 30px; padding: 0 9px; border: 1px solid #d7e0e8; border-radius: 7px; background: #fff; color: #526577; font-size: 13px; font-weight: 700; cursor: pointer; }
.library-search button:hover { background: #eef4f8; color: #2c6488; }
.panel-heading { min-height: 48px; box-sizing: border-box; padding: 13px 12px; display: flex; justify-content: space-between; align-items: center; gap: 8px; }.panel-heading strong { font-size: 13px; }.panel-heading>.heading-meta,.scene-inspector .panel-heading>span { font-size: 10px; color: #93a1ae; }
.section-toggle { width: 100%; flex-shrink: 0; background: transparent; text-align: left; transition: background .15s, color .15s; }
.section-toggle:hover { background: #eef4f8; }
.section-toggle:focus-visible { box-shadow: inset 0 0 0 2px #7aa8c3; }
.heading-title { display: inline-flex; flex: 0 0 auto; align-items: center; gap: 7px; color: #25394b; white-space: nowrap; }
.heading-meta { min-width: 0; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
.heading-title i { width: 12px; color: #6d8799; font-size: 13px; font-style: normal; text-align: center; }
.library-section-body { flex: 0 1 auto; min-height: 0; overflow-y: auto; padding: 12px 0 16px; }
.scene-devices-body { display: flex; flex: 1; flex-direction: column; min-height: 0; border-top: 1px solid #edf1f4; padding-top: 10px; }
.model-library { padding: 0 14px; display: grid; grid-template-columns: 1fr 1fr; gap: 8px; }
.model-card { display: flex; flex-direction: column; align-items: center; justify-content: center; gap: 4px; height: 72px; background: #f9fbfc; border: 1px solid #dfe7ed; border-radius: 8px; color: #526577; font-size: 11px; cursor: grab; transition: transform .15s, background .15s, border-color .15s, box-shadow .15s; }.model-card:hover { transform: translateY(-1px); background: #f0f7fb; border-color: #9bbfd3; box-shadow: 0 4px 10px #3e647b14; }.model-icon { font-size: 31px; line-height: 1; }
.tree-heading { border: 0; margin: 0; background: #fff; }.scene-search { width: calc(100% - 20px); margin: 0 10px 9px; height: 32px; min-height: 32px; border-radius: 6px; border: 1px solid #dce4eb; padding: 0 9px; background: #f9fbfc; font-size: 11px; }
.scene-tree { flex: 1; min-height: 80px; overflow-y: auto; padding: 0 6px 12px; scrollbar-gutter: stable; }.tree-row { display: flex; align-items: center; height: 35px; gap: 3px; border-radius: 6px; }.tree-row:hover { background: #f3f7fa; }.tree-row.selected { background: #e6eff6; }.tree-row.hidden { opacity: .55; }.tree-row button { background: transparent; border: 0; cursor: pointer; font-size: 12px; color: #7e92a3; padding: 4px; }.tree-select { min-width: 0; flex: 1; display: flex; align-items: center; gap: 7px; text-align: left; }.tree-select span:last-child { overflow: hidden; white-space: nowrap; text-overflow: ellipsis; color: #4c6072; font-size: 11px; }
.library-foot { border-top: 1px solid var(--scene-border); padding: 13px 10px; font-size: 10px; color: #91a0ad; text-align: center; }
.scene-center { flex: 1; min-width: 0; min-height: 0; display: flex; flex-direction: column; }.scene-stage { flex: 1; min-height: 160px; position: relative; overflow: hidden; background: #e7edf1; }.scene-render-host { position: absolute; inset: 0; }
.viewport-toolbar { position: absolute; left: 16px; top: 15px; display: flex; gap: 8px; flex-wrap: wrap; right: 85px; }.viewport-tool-group { display: flex; padding: 3px; border: 1px solid #ffffffa8; border-radius: 7px; background: #ffffffd9; box-shadow: 0 3px 12px #233e5210; backdrop-filter: blur(10px); }.viewport-tool-group button { border: 0; background: transparent; color: #627586; font-size: 14px; padding: 7px 11px; border-radius: 4px; cursor: pointer; }.viewport-tool-group button.active { background: #e0ebf3; color: #2c6488; }
.scene-history { position: absolute; right: 14px; top: 15px; display: flex; gap: 4px; }.scene-history button { width: 28px; height: 31px; border: 1px solid #fff9; border-radius: 5px; background: #ffffffdb; color: #526b7e; font-size: 20px; cursor: pointer; }.scene-history button:disabled { opacity: .4; cursor: default; }
.scene-empty { position: absolute; top: 50%; left: 50%; transform: translate(-50%, -50%); width: min(400px, 85%); text-align: center; background: #f9fcffed; border: 1px solid #fff; padding: 26px; border-radius: 14px; box-shadow: 0 12px 50px #314e6212; }.scene-empty strong { display: block; font-size: 20px; font-weight: 600; }.scene-empty p { color: #8092a0; font-size: 12px; margin: 12px 0 20px; line-height: 1.8; }.empty-cube { display: block; font-size: 50px; color: #7197b1; margin-bottom: 15px; }
.scene-compass { position: absolute; right: 23px; bottom: 42px; display: flex; flex-direction: column; align-items: center; gap: 6px; color: #6f899b; font: 12px var(--font-family); }.scene-compass b { color: #508d74; }.scene-legend { position: absolute; bottom: 42px; left: 17px; display: flex; gap: 15px; color: #637b8e; font-size: 13px; }.scene-legend span { display: flex; align-items: center; gap: 6px; }.scene-navigation-hint { position: absolute; left: 17px; bottom: 16px; color: #778b99; font-size: 13px; }
.state-dot,.scene-legend i { display: inline-block; width: 6px; height: 6px; min-width: 6px; border-radius: 50%; background: #91a0ad; }[data-state="running"],[data-state="good"] { background-color: #2aaf83; }[data-state="fault"] { background-color: #dd5260; }[data-state="missing"],[data-state="stale"] { background-color: #c28b3b; }
.scene-data-dock { height: 172px; min-height: 70px; flex-shrink: 0; border-top: 1px solid var(--scene-border); background: #fff; display: flex; flex-direction: column; font-size: 14px; }.data-tabs { display: flex; align-items: center; gap: 16px; border-bottom: 1px solid #edf0f4; padding: 0 16px; height: 42px; flex-shrink: 0; }.data-tabs button { height: 42px; background: transparent; border: 0; border-bottom: 2px solid transparent; font-size: 14px; color: #718695; cursor: pointer; }.data-tabs button.active { color: #396d90; border-bottom-color: #396d90; }.data-tabs button>span { margin-left: 5px; padding: 2px 6px; background: #f0f4f7; border-radius: 3px; font-size: 11px; }.shared-data-note { margin-left: auto; color: #8798a5; font-size: 12px; white-space: nowrap; }
.device-status-list { display: grid; grid-template-columns: repeat(auto-fill, minmax(190px, 1fr)); grid-auto-rows: minmax(72px, auto); gap: 9px; padding: 11px 16px 14px; flex: 1; min-height: 0; overflow-x: hidden; overflow-y: auto; align-content: start; overscroll-behavior: contain; }
.device-status-card { position: relative; min-width: 0; display: grid; grid-template-columns: minmax(0, 1fr) auto; grid-template-areas: 'name value' 'meta state'; align-items: center; column-gap: 10px; row-gap: 7px; border: 1px solid #e0e7ed; border-left: 4px solid #91a0ad; border-radius: 7px; background: #fbfcfd; padding: 10px 12px 9px 10px; text-align: left; cursor: pointer; transition: border-color .15s, background .15s, box-shadow .15s, transform .15s; }
.device-status-card:hover { border-color: #b7cad6; background: #f7fbfd; box-shadow: 0 3px 10px #34506512; transform: translateY(-1px); }
.device-status-card.selected { border-color: #72a8c6; border-left-color: #397fa7; background: #eef7fc; box-shadow: 0 0 0 1px #72a8c633; }
.device-status-card[data-card-state="running"],.device-status-card[data-card-state="good"] { border-left-color: #2aaf83; }
.device-status-card[data-card-state="fault"] { border-left-color: #dd5260; background: #fff8f9; }
.device-status-card[data-card-state="offline"] { border-left-color: #8292a3; }
.device-status-card[data-card-state="missing"],.device-status-card[data-card-state="stale"] { border-left-color: #c28b3b; background: #fffaf2; }
.device-status-name { grid-area: name; min-width: 0; display: flex; gap: 7px; align-items: center; color: #526b7e; font-size: 13px; }
.device-status-name b { min-width: 0; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; font-weight: 600; }
.device-status-card>strong { grid-area: value; max-width: 110px; overflow: hidden; color: #345c77; font: 20px var(--font-family); line-height: 1; text-overflow: ellipsis; white-space: nowrap; }
.device-status-meta { grid-area: meta; min-width: 0; overflow: hidden; color: #8799a6; font-size: 12px; text-overflow: ellipsis; white-space: nowrap; }
.device-status-card>small { grid-area: state; color: #718795; font-size: 12px; white-space: nowrap; }
.muted-empty { padding: 20px 12px; color: #7d909e; font-size: 13px; text-align: center; width: 100%; }.scene-alarm-list { overflow: auto; flex: 1; }.scene-alarm-list button { width: 100%; display: flex; align-items: center; gap: 12px; padding: 10px 16px; background: #fff; border: 0; border-bottom: 1px solid #edf0f4; text-align: left; cursor: pointer; color: #637b8d; font-size: 13px; }.scene-alarm-list small { margin-left: auto; font-size: 12px; }.scene-trend { min-height: 0; flex: 1; }
.scene-data-dock :deep(.trend-legend-item), .scene-data-dock :deep(.no-data) { font-size: 13px; }
.scene-inspector { width: 264px; flex: 0 0 264px; background: #fff; border-left: 1px solid var(--scene-border); display: flex; flex-direction: column; min-height: 0; }.scene-inspector .panel-heading { border-bottom: 1px solid #edf0f4; }.inspector-scroll { padding: 16px 16px 24px; overflow-y: auto; flex: 1; }.inspector-close { background: transparent; border: 0; font-size: 20px; color: #8fa0ad; cursor: pointer; }
.device-summary { padding: 18px 0 15px; }.summary-type { font-size: 10px; color: #8ca0af; }.device-summary h3 { font-size: 16px; margin: 6px 0 9px; color: #354f64; font-weight: 600; }.device-summary>div { display: flex; gap: 6px; align-items: center; font-size: 11px; color: #8b9ba8; }.device-summary>strong { display: block; margin: 14px 0 4px; font: 29px var(--font-family); color: #3d6b8a; }.device-summary>small { color: #a2afba; font-size: 10px; }
.inspector-focus { width: 100%; padding: 8px; margin-bottom: 16px; border: 1px solid #d6e5ef; color: #48718d; background: #f3f8fb; border-radius: 5px; font-size: 11px; cursor: pointer; }.inspector-field { display: flex; flex-direction: column; gap: 7px; color: #7a8c9b; font-size: 11px; margin-bottom: 13px; }.inspector-field>input,.inspector-field>select { min-height: 32px; height: 32px; width: 100%; padding: 0 8px; border-radius: 5px; border: 1px solid #dfe6ec; background: #fbfcfd; font-size: 11px; color: #435d72; }.inspector-section-title { font-size: 12px; color: #455f74; padding-top: 16px; border-top: 1px solid #edf0f4; margin: 18px 0 13px; display: flex; justify-content: space-between; }.inspector-section-title>span { font-size: 9px; color: #9eabb5; }
.vector-inputs { display: flex; gap: 5px; }.vector-inputs>label { display: flex; align-items: center; flex: 1; min-width: 0; border: 1px solid #dfe6ec; border-radius: 5px; padding-left: 6px; background: #fbfcfd; }.vector-inputs span { font: 10px var(--font-family); }.axis-X { color: #bf7275; }.axis-Y { color: #59997d; }.axis-Z { color: #678fba; }.vector-inputs input { width: 100%; min-width: 0; height: 30px; min-height: 30px; border: 0; padding: 0 2px 0 5px; background: transparent; font-size: 10px; color: #486278; }.color-field { flex-direction: row; align-items: center; justify-content: space-between; }.color-field input { width: 52px; padding: 3px; }.range-fields { display: flex; gap: 8px; }.range-fields>label { min-width: 0; flex: 1; }.binding-search { width: 100%; margin: 0 0 12px; }.field-note { font-size: 10px; line-height: 1.85; color: #99a7b2; margin: 0 0 16px; }.inspector-node-actions { display: flex; flex-wrap: wrap; gap: 5px; padding-top: 14px; border-top: 1px solid #edf0f4; }.inspector-node-actions .btn { font-size: 10px; }
.inspector-check { display: flex; align-items: center; gap: 8px; margin: 16px 0; color: #71899b; font-size: 11px; }.inspector-check input { width: 14px; height: 14px; min-height: 0; accent-color: #5285a5; }.scene-counts { display: flex; gap: 10px; margin: 25px 0 16px; }.scene-counts>div { flex: 1; background: #f8fafc; padding: 15px 10px; border: 1px solid #e7edf2; border-radius: 6px; display: flex; flex-direction: column; gap: 6px; }.scene-counts strong { font: 24px var(--font-family); color: #5b7a91; }.scene-counts span { font-size: 10px; color: #99a9b6; }.clear-scene { margin-top: 15px; }.runtime-binding { display: flex; flex-direction: column; gap: 7px; margin-bottom: 18px; font-size: 11px; color: #91a2af; }.runtime-binding strong { color: #526f84; font-weight: 500; }
.is-running { position: fixed; inset: 0; z-index: 1100; }.theme-dark .scene-stage { background: #142333; }.theme-dark .scene-legend { color: #9cafbf; }.theme-dark .scene-navigation-hint { color: #7790a4; }
:deep(.industrial-webgl) { display: block; width: 100%; height: 100%; outline: none; touch-action: none; }
:deep(.industrial-label-layer) { position: absolute; inset: 0; pointer-events: none; overflow: hidden; }
:deep(.industrial-device-label) { pointer-events: auto; border: 1px solid #ffffffed; border-left: 3px solid #9bafbc; border-radius: 5px; background: #ffffffed; color: #435e72; padding: 7px 10px; min-width: 105px; max-width: min(520px, 60vw); box-shadow: 0 4px 15px #263c5014; text-align: left; cursor: pointer; }
:deep(.industrial-device-label strong) { display: block; font-size: 14px; font-weight: 600; line-height: 1.25; overflow-wrap: anywhere; white-space: normal; }
:deep(.industrial-device-label span) { display: block; margin-top: 4px; color: #869caa; font: 9px var(--font-family); }
:deep(.industrial-device-label.selected) { border-color: #5e9fc4; box-shadow: 0 0 0 2px #5e9fc422; }
:deep(.industrial-device-label[data-state="running"]),:deep(.industrial-device-label[data-state="good"]) { border-left-color: #2aaf83; }
:deep(.industrial-device-label[data-state="fault"]) { border-left-color: #dd5260; }
:deep(.industrial-device-label[data-state="stale"]),:deep(.industrial-device-label[data-state="missing"]) { border-left-color: #c28b3b; }
.theme-dark :deep(.industrial-device-label) { background: #21394df0; border-color: #456178; color: #d6e5ef; }.theme-dark :deep(.industrial-device-label span) { color: #a4bbc9; }
@media (max-width: 1200px) { .scene-library { width: 190px; flex-basis: 190px; }.scene-inspector { width: 232px; flex-basis: 232px; }.shared-data-note { display: none; } }
@media (max-width: 900px) { .scene-library { width: 152px; flex-basis: 152px; }.library-header { padding-inline: 14px; }.library-search { padding-inline: 8px; }.model-library { padding: 0 8px; gap: 5px; }.model-card { height: 63px; font-size: 10px; }.scene-inspector { width: 212px; flex-basis: 212px; }.inspector-scroll { padding: 0 12px 20px; }.scene-legend { display: none; }.viewport-toolbar { left: 8px; top: 9px; }.viewport-tool-group button { padding: 6px 8px; }.panel-heading>.heading-meta { display: none; } }
/* Sidebar text stays readable independently of panel width and scene zoom. */
.scene-library, .scene-inspector { font-size: 14px; }
.scene-library .panel-heading strong, .scene-inspector .panel-heading strong { font-size: 15px; }
.scene-library .panel-heading > .heading-meta, .scene-inspector .panel-heading > span { font-size: 12px; }
.scene-library .model-card { height: 82px; font-size: 14px; }
.scene-library .tree-row { height: 40px; }
.scene-library .tree-select span:last-child, .scene-library .muted-empty { font-size: 14px; }
.scene-library .scene-search, .scene-inspector .scene-search { font-size: 14px; height: 36px; }
.scene-inspector .inspector-field, .scene-inspector .inspector-check, .scene-inspector .inspector-focus { font-size: 14px; }
.scene-inspector .inspector-field > input, .scene-inspector .inspector-field > select { font-size: 14px; height: 36px; }
.scene-inspector .vector-inputs input, .scene-inspector .vector-inputs span { font-size: 13px; }
.scene-inspector .inspector-section-title { font-size: 15px; }
.scene-inspector .inspector-section-title > span, .scene-inspector .summary-type, .scene-inspector .device-summary > small { font-size: 12px; }
.scene-inspector .device-summary h3 { font-size: 18px; }
.scene-inspector .device-summary > div, .scene-inspector .scene-counts span, .scene-inspector .btn { font-size: 14px; }
.scene-inspector .field-note { font-size: 13px; }
.label-size-reset { margin: -4px 0 14px; color: #48718d; font-size: 13px; text-decoration: underline; }
.viewport-tool-group button { font-size: 14px; }
.scene-navigation-hint { font-size: 13px; }
</style>
