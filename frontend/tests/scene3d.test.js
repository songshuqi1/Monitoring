import test from 'node:test'
import assert from 'node:assert/strict'
import { createPinia, setActivePinia } from 'pinia'
import { normalizeScene, emptyScene, createSceneNode, nodeRuntime, sceneExport, parseSceneFile, projectVariableReferences, validateImportedProjectBindings } from '../src/services/scene3dService.js'
import { fixtureScene } from './fixtures/scene3dFixture.js'
import api from '../src/api/index.js'
import { normalizeScadaProjectImport } from '../src/services/scadaProjectImport.js'
import { projectService } from '../src/services/projectService.js'
import { useMonitorStore } from '../src/store/index.js'

const data = new Map()
globalThis.localStorage = { getItem: k => data.get(k) ?? null, setItem: (k, v) => data.set(k, String(v)), removeItem: k => data.delete(k) }
api.getCustomComponents = async () => ({ data: [] })

test('scene survives a portable JSON round trip without any asset cache', () => {
  const scene = fixtureScene()
  scene.environment.theme = 'dark'
  scene.environment.labelFontSize = 18
  scene.nodes[0].labelFontSize = 26
  scene.camera = { position: [10, 12, 14], target: [2, 1, 0] }
  const exported = sceneExport(scene, '泵站', [], [])
  const imported = parseSceneFile(JSON.parse(JSON.stringify(exported)), [])
  assert.deepEqual(imported.scene, scene)
  assert.equal(imported.unresolved, 0)
})

test('standalone export resolves live 2D binding and rejects colliding variable ids', () => {
  const scene = emptyScene()
  scene.nodes.push(createSceneNode('pump', { linkedWidgetId: 'w-1', bindings: { status: 'old' } }))
  const variables = [{ id: 7, name: '泵运行', source: 'OPCUA', resourceId: 'r-1' }]
  const file = sceneExport(scene, '泵站', variables, [{ id: 'w-1', config: { varId: 7 } }])
  assert.equal(file.scene3d.nodes[0].linkedWidgetId, null)
  assert.equal(file.scene3d.nodes[0].bindings.status, '7')
  assert.equal(parseSceneFile(file, variables).unresolved, 0)
  const collision = parseSceneFile(file, [{ ...variables[0], name: '另一台泵' }])
  assert.equal(collision.unresolved, 1)
  assert.equal(collision.scene.nodes[0].bindings.status, 'unresolved:7')
})

test('invalid and future scenes are rejected before mutation', () => {
  assert.throws(() => normalizeScene({ version: 8, nodes: [] }), /版本/)
  assert.throws(() => normalizeScene({ version: 1 }), /nodes/)
  assert.throws(() => normalizeScene({ nodes: [{ type: 'script' }] }), /不支持/)
  const node = createSceneNode('tank')
  assert.throws(() => normalizeScene({ nodes: [node, node] }), /重复/)
  assert.throws(() => normalizeScene({ nodes: [{ ...node, range: { min: 10, max: 0 } }] }), /量程/)
  assert.throws(() => normalizeScene({ nodes: Array(1001).fill(node) }), /1000/)
})

test('existing 2D and legacy graph imports still work and v2 preserves 3D', () => {
  const layout = { widgets: [{ id: 'w-1', type: 'processPump', config: { varId: 7 } }], connections: [] }
  assert.equal(normalizeScadaProjectImport({ type: 'monitoring-platform-project', version: 1, project: { name: '旧项目', layout } }).layout.widgets.length, 1)
  const imported = normalizeScadaProjectImport({ type: 'monitoring-platform-project', version: 2, project: { name: '完整项目', layout: { ...layout, scene3d: fixtureScene() } } })
  assert.equal(imported.layout.scene3d.nodes.length, 10)
  assert.equal(normalizeScadaProjectImport({ graph: { cells: [] } }).layout.widgets.length, 0)
  assert.throws(() => normalizeScadaProjectImport({ type: 'monitoring-platform-project', version: 100 }), /版本/)
})

test('real-time state distinguishes missing, offline, stale, fault, running and stopped', () => {
  const node = createSceneNode('pump', { bindings: { status: '7', fault: '8' } })
  const now = 100000
  const store = { variables: [{ id: 7 }, { id: 8 }], widgets: [], realtimeData: { 7: { value: 1, quality: 'GOOD', timestamp: now }, 8: { value: 0, quality: 'GOOD', timestamp: now } } }
  assert.equal(nodeRuntime(node, store, now).state, 'running')
  store.realtimeData[7].value = 0
  assert.equal(nodeRuntime(node, store, now).state, 'stopped')
  store.realtimeData[8].value = 1
  assert.equal(nodeRuntime(node, store, now).state, 'fault')
  assert.equal(nodeRuntime(node, store, now + 20000).state, 'stale')
  store.realtimeData[7].quality = 'BAD'
  assert.equal(nodeRuntime(node, store, now).state, 'offline')
  store.variables = []
  assert.equal(nodeRuntime(node, store, now).state, 'missing')
  assert.equal(nodeRuntime(createSceneNode('tank'), store, now).state, 'unbound')
})

test('liquid level is clamped to range and never animates on stale data', () => {
  const node = createSceneNode('tank', { bindings: { value: '1' }, range: { min: 20, max: 80 } })
  const store = { variables: [{ id: 1, unit: '%' }], widgets: [], realtimeData: { 1: { value: 50, quality: 'GOOD', timestamp: 100000 } } }
  assert.equal(nodeRuntime(node, store, 100000).level, 0.5)
  store.realtimeData[1].value = 100
  assert.equal(nodeRuntime(node, store, 100000).level, 1)
  assert.equal(nodeRuntime(node, store, 120000).level, 0)
})

test('project save, copy and reopen preserve both layouts', async () => {
  data.clear()
  const layout = { widgets: [{ id: 'w1', type: 'processPump' }], connections: [], scene3d: fixtureScene() }
  const project = await projectService.createProject({ name: '组合项目', layout })
  const copied = await projectService.duplicateProject(project.id)
  assert.deepEqual(copied.layout, project.layout)
  assert.notEqual(copied.id, project.id)
  await projectService.updateProject(project.id, { name: '组合项目', layout: { ...layout, widgets: [] } })
  const reopened = await projectService.getProject(project.id)
  assert.equal(reopened.layout.scene3d.nodes.length, 10)
  assert.equal(reopened.layout.widgets.length, 0)
})

test('project import validates both 2D and 3D variable identities without creating new resources', () => {
  const variables = [{ id: 7, name: '泵运行', source: 'OPCUA', resourceId: 'r-1', opcuaNodeId: 'ns=1;s=Pump' }]
  const layout = { widgets: [{ id: 'w-1', type: 'processPump', config: { varId: 7 } }], connections: [], scene3d: emptyScene() }
  layout.scene3d.nodes.push(createSceneNode('pump', { linkedWidgetId: 'w-1', bindings: { fault: '7' } }))
  const references = projectVariableReferences(layout, variables)
  assert.equal(references.length, 1)
  assert.deepEqual(validateImportedProjectBindings(layout, references, variables).layout, layout)
  const imported = validateImportedProjectBindings(layout, references, [{ ...variables[0], opcuaNodeId: 'ns=1;s=DifferentPump' }])
  assert.equal(imported.unresolved, 2)
  assert.equal(imported.layout.widgets[0].config.varId, 'unresolved:7')
  assert.equal(imported.layout.scene3d.nodes[0].bindings.fault, 'unresolved:7')
  assert.equal(layout.widgets[0].config.varId, 7, 'validation never mutates the source')
})

test('a corrupt project cannot cause the existing project library to be overwritten', async () => {
  data.clear()
  data.set('monitor-projects', '{corrupt-json')
  await assert.rejects(projectService.createProject({ name: '新项目', layout: {} }), /原始数据已保留/)
  assert.equal(data.get('monitor-projects'), '{corrupt-json')
})

test('scene undo is independent of 2D; switching projects preserves drafts and resets history', () => {
  data.clear(); setActivePinia(createPinia())
  const store = useMonitorStore()
  store.replaceLayout({ widgets: [{ id: 'w-1', type: 'processPump', config: {} }], connections: [], scene3d: emptyScene() })
  store.markProjectSaved('a')
  store.replaceScene(fixtureScene())
  store.undoScene()
  assert.equal(store.scene3d.nodes.length, 0)
  assert.equal(store.widgets.length, 1)
  store.redoScene()
  assert.equal(store.scene3d.nodes.length, 10)
  store.activateProject({ id: 'b', layout: { widgets: [], connections: [] } })
  assert.equal(store.scene3d.nodes.length, 0)
  assert.equal(store.canUndoScene, false)
  assert.equal(store.canUndo, false)
  store.activateProject({ id: 'a', layout: { widgets: [], connections: [] } })
  assert.equal(store.scene3d.nodes.length, 10)
  assert.equal(store.widgets.length, 1)
  store.persistLayout()
  setActivePinia(createPinia())
  const restored = useMonitorStore()
  restored.loadLayout()
  assert.equal(restored.scene3d.nodes.length, 10)
  assert.equal(restored.draftProjectId, 'a')
})

test('upgrading an existing 2D workspace retains its unsaved current-project draft', () => {
  data.clear(); setActivePinia(createPinia())
  data.set('monitor-current-project-id', 'legacy-project')
  data.set('monitor-widget-layout', JSON.stringify({ widgets: [{ id: 'w-legacy', type: 'processPump', x: 123, config: {} }], connections: [] }))
  const store = useMonitorStore()
  store.loadLayout()
  assert.equal(store.draftProjectId, 'legacy-project')
  assert.equal(store.widgets[0].x, 123)
  assert.deepEqual(store.scene3d, emptyScene())
})
