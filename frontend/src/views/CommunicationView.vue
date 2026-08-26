<template>
  <div class="comm-view">
    <div class="cv-header">
      <h2>通信资源</h2>
      <p class="cv-header-desc">配置 OPC UA / UDP 数据源，系统将从远程节点读取数据并自动创建变量</p>
      <div v-if="operationMsg" class="cv-operation-msg" :class="operationMsg.ok ? 'ok' : 'err'">{{ operationMsg.text }}</div>
      <div v-if="resourceLoadFailed" class="cv-operation-msg err">
        通信资源未能加载。{{ resourceLoadError }}
        <button class="btn btn-xs" :disabled="loadingResources" @click="loadResources">
          {{ loadingResources ? '重新加载中…' : '重新加载' }}
        </button>
      </div>
    </div>

    <div class="cv-scope-filter">
      <label>显示范围</label>
      <select v-model="scopeFilter">
        <option value="all">全部项目 / 应用</option>
        <option value="global">公共资源</option>
        <option v-for="scope in scopeOptions" :key="scope.id" :value="scope.id">{{ scope.name }}</option>
      </select>
      <span>当前显示 {{ visibleResources.length }} / {{ resources.length }} 个通信资源</span>
    </div>

    <!-- 资源列表 -->
    <div class="cv-list">
      <div v-for="res in visibleResources" :key="res.id" class="cv-card" :class="'type-' + res.type">
        <div class="cv-card-header">
          <div class="cv-card-title">
            <span class="cv-type-badge" :class="res.type">{{ res.type === 'OPCUA' ? 'OPC UA' : res.type }}</span>
            <span class="cv-name">{{ res.name }}</span>
          </div>
          <div class="cv-card-actions">
            <span class="cv-status" :class="res.running ? 'running' : ''">
              <span class="cv-status-dot"></span>
              {{ res.running ? '采集中' : '已停止' }}
            </span>
            <button v-if="res.type === 'OPCUA' || res.type === 'UDP' || res.type === 'SDC'" class="btn btn-sm" :class="res.running ? 'btn-danger' : 'btn-primary'"
              :disabled="resourceBusyId === res.id" @click="togglePoll(res)">
              {{ resourceBusyId === res.id ? '处理中...' : (res.running ? '停止' : '启动采集') }}
            </button>
            <button class="btn btn-sm" @click="editResourceById(res.id)">编辑</button>
            <button class="btn btn-sm" @click="deleteResourceById(res.id)">删除</button>
          </div>
        </div>
        <div class="cv-card-body">

          <div class="cv-detail-row">
            <span class="cv-detail-label">所属范围</span>
            <span class="cv-scope-tag">{{ resourceScopeLabel(res) }}</span>
          </div>

          <!-- OPC UA 详情 -->
          <template v-if="res.type === 'OPCUA'">
            <div class="cv-detail-row">
              <span class="cv-detail-label">端点</span>
              <code class="cv-detail-val">{{ res.endpoint }}</code>
            </div>
            <div class="cv-detail-row">
              <span class="cv-detail-label">采集间隔</span>
              <code class="cv-detail-val">{{ res.pollIntervalMs || 1000 }} ms</code>
            </div>
            <div class="cv-nodes-header">
              <span class="cv-nodes-title">读取节点 ({{ (res.nodes || []).length }})</span>
              <button class="btn btn-xs" @click="addNodeToResourceById(res.id)">＋ 添加节点</button>
            </div>
            <div class="cv-nodes-list">
              <div v-for="(node, ni) in (res.nodes || [])" :key="ni" class="cv-node-row">
                <span class="cv-node-idx">{{ ni + 1 }}</span>
                <div class="cv-node-info">
                  <span class="cv-node-name">{{ node.name }}</span>
                  <code class="cv-node-nid">{{ node.nodeId }}</code>
                </div>
                <div class="cv-node-value" v-if="getNodeValue(node.name) !== null">
                  <span class="cv-nv-val">{{ getNodeValue(node.name) }}</span>
                  <span class="quality-dot" :class="getNodeQuality(node.name)"></span>
                </div>
                <button class="btn btn-sm" @click="removeNodeFromResourceById(res.id, ni)" title="移除">✕</button>
              </div>
            </div>
            <div v-if="!res.nodes || res.nodes.length === 0" class="cv-nodes-empty">
              暂无节点，点击「+ 添加节点」添加要读取的 OPC UA 节点
            </div>
          </template>

          <!-- UDP 详情 -->
          <template v-if="res.type === 'UDP'">
              <div class="cv-detail-row">
                <span class="cv-detail-label">绑定地址</span>
                <code class="cv-detail-val">{{ res.address || '0.0.0.0' }}</code>
              </div>
            <div class="cv-detail-row">
              <span class="cv-detail-label">监听端口</span>
              <code class="cv-detail-val">:{{ res.port }}</code>
            </div>
          </template>

          <template v-if="res.type === 'SDC'">
            <div class="cv-detail-row">
              <span class="cv-detail-label">服务地址</span>
              <code class="cv-detail-val">{{ res.sdcUrl }}</code>
            </div>
            <div class="cv-detail-row">
              <span class="cv-detail-label">采集间隔</span>
              <code class="cv-detail-val">{{ res.pollIntervalMs || 1000 }} ms</code>
            </div>
            <p class="cv-sdc-hint">轮询 <code>/api/runtime/stream</code>，读取返回 JSON 的 <code>vars</code> 对象；数值和布尔变量会自动加入变量管理。</p>
          </template>

        </div>
      </div>

      <!-- 空状态 -->
      <div v-if="visibleResources.length === 0" class="cv-empty">
        <div class="cv-empty-icon">📡</div>
        <p>还没有通信资源</p>
        <p class="cv-empty-hint">添加 OPC UA 或 UDP 数据源来采集数据</p>
      </div>
    </div>

    <!-- 添加/编辑按钮 -->
    <div class="cv-actions">
      <button class="btn btn-primary" @click="openAddDialog">＋ 添加通信资源</button>
    </div>

    <!-- 添加/编辑对话框 -->
    <div v-if="showDialog" class="dialog-overlay" @click.self="showDialog = false">
      <div class="dialog dialog-lg" @click.stop>
        <h3>{{ editingIdx >= 0 ? '编辑通信资源' : '添加通信资源' }}</h3>

        <div class="form-group">
          <label>名称</label>
          <input v-model="form.name" placeholder="例如: 1号PLC" />
        </div>
        <div class="form-group">
          <label>类型</label>
          <select v-model="form.type">
            <option value="OPCUA">OPC UA (远程读取)</option>
            <option value="UDP">UDP (监听端口)</option>
            <option value="SDC">软件定义通信 (Monitor Agent)</option>
          </select>
        </div>

        <div class="form-group">
          <label>所属范围</label>
          <select v-model="form.scopeType">
            <option value="global">公共资源（所有项目可见）</option>
            <option value="project">指定项目</option>
            <option value="application">独立应用</option>
          </select>
        </div>
        <div v-if="form.scopeType === 'project'" class="form-group">
          <label>项目</label>
          <select v-model="form.projectId">
            <option value="">请选择项目</option>
            <option v-for="project in projectStore.list" :key="project.id" :value="String(project.id)">{{ project.name }}</option>
          </select>
          <p class="form-hint">资源与采集变量会归入这个项目，可在数据视图中单独筛选。</p>
        </div>
        <div v-if="form.scopeType === 'application'" class="form-group">
          <label>应用名称</label>
          <input v-model.trim="form.applicationName" placeholder="例如：一号产线应用" />
          <p class="form-hint">相同名称的资源与变量会被归入同一个应用范围。</p>
        </div>

        <template v-if="form.type === 'OPCUA'">
          <div class="form-group">
            <label>端点 URL</label>
            <input v-model="form.endpoint" placeholder="opc.tcp://192.168.1.100:4840" />
            <p class="form-hint">本机发布端只监听 127.0.0.1 时请用 opc.tcp://127.0.0.1:4840；远程发布端需监听远程网卡 IP，并放行端口。</p>
          </div>
          <div class="form-group">
            <label>节点列表</label>
              <p class="form-hint">每个节点一行，支持 ns=2;i=2；字符串 JSON 字典节点会自动拆出 temperature、humidity、pressure 等数值字段。</p>
              <textarea v-model="form.nodesText" rows="4"
                placeholder="ns=2;i=1&#10;ns=1;s=temperature_1&#10;ns=1;s=pressure_1"></textarea>
          </div>
          <div class="comm-connect-bar">
            <button class="btn btn-sm" @click="testConn" :disabled="testLoading">
              {{ testLoading ? '测试中...' : '🔌 测试连接' }}
            </button>
            <span v-if="testMsg" class="test-msg" :class="testMsg.ok ? 'ok' : 'err'">{{ testMsg.text }}</span>
          </div>
        </template>

        <template v-if="form.type === 'UDP'">
            <div class="form-group">
              <label>绑定地址</label>
              <input v-model="form.address" placeholder="0.0.0.0 (所有网卡)" />
              <p class="form-hint">指定监听哪个网卡 IP，0.0.0.0 表示所有网卡</p>
            </div>
          <div class="form-group">
            <label>监听端口</label>
            <input type="number" v-model.number="form.port" placeholder="8888" />
          </div>
        </template>

        <template v-if="form.type === 'SDC'">
          <div class="form-group">
            <label>服务地址</label>
            <input v-model="form.sdcUrl" placeholder="http://127.0.0.1:9100" />
            <p class="form-hint">填写 Monitor Agent 根地址或完整的 <code>/api/runtime/stream</code> 地址。系统会直连该服务，不使用系统代理。</p>
          </div>
          <div class="comm-connect-bar">
            <button class="btn btn-sm" @click="testConn" :disabled="testLoading">
              {{ testLoading ? '测试中...' : '测试 SDC 连接' }}
            </button>
            <span v-if="testMsg" class="test-msg" :class="testMsg.ok ? 'ok' : 'err'">{{ testMsg.text }}</span>
          </div>
        </template>

        <div class="form-group">
          <label>采集间隔 (ms)</label>
          <input type="number" v-model.number="form.interval" placeholder="1000" />
        </div>

        <div class="dialog-actions">
          <button class="btn btn-secondary" @click="showDialog = false">取消</button>
          <button class="btn btn-primary" :disabled="savingResource" @click="saveResource">{{ savingResource ? '保存中...' : '保存' }}</button>
        </div>
      </div>
    </div>

    <!-- 添加节点到资源对话框 -->
    <div v-if="showNodeDialog" class="dialog-overlay" @click.self="showNodeDialog = false">
      <div class="dialog" @click.stop>
        <h3>添加读取节点</h3>
        <div class="form-group">
          <label>节点名称</label>
          <input v-model="nodeForm.name" placeholder="例如: temperature_1" />
        </div>
        <div class="form-group">
          <label>NodeId</label>
          <input v-model="nodeForm.nodeId" placeholder="ns=1;s=temperature_1" />
          <p class="form-hint">从远程 OPC UA 服务器读取此节点的值</p>
        </div>
        <div class="node-preview">
          <code>\Root\Objects\{{ nodeForm.name || '名称' }}</code>
        </div>
        <div class="dialog-actions">
          <button class="btn btn-secondary" @click="showNodeDialog = false">取消</button>
          <button class="btn btn-primary" @click="confirmAddNode">添加</button>
        </div>
      </div>
    </div>

    <ConfirmDialog
      v-if="deleteTargetIdx >= 0"
      title="删除通信资源"
      :message="`删除通信资源「${resources[deleteTargetIdx]?.name || ''}」？`"
      detail="如果资源正在采集，系统会先停止采集再删除本地配置。"
      confirm-text="确认删除"
      danger
      @close="deleteTargetIdx = -1"
      @confirm="confirmDeleteResource"
    />
  </div>
</template>

<script>
import { ref, reactive, computed, onMounted } from 'vue'
import { useMonitorStore } from '../store/index.js'
import { useProjectStore } from '../store/projectStore.js'
import api from '../api/index.js'
import ConfirmDialog from '../components/common/ConfirmDialog.vue'

export default {
  name: 'CommunicationView',
  components: { ConfirmDialog },
  setup() {
    const store = useMonitorStore()
    const projectStore = useProjectStore()
    const resources = ref([])
    const scopeFilter = ref('all')
    const showDialog = ref(false)
    const editingIdx = ref(-1)
    const testLoading = ref(false)
    const testMsg = ref(null)
    const showNodeDialog = ref(false)
    const nodeTargetIdx = ref(-1)
    const nodeForm = reactive({ name: '', nodeId: '' })
    const deleteTargetIdx = ref(-1)
    const operationMsg = ref(null)
    const resourceBusyId = ref('')
    const savingResource = ref(false)
    const loadingResources = ref(false)
    const resourceLoadFailed = ref(false)
    const resourceLoadError = ref('')

    const form = reactive({
      name: '', type: 'OPCUA', endpoint: '', nodesText: '', address: '0.0.0.0', sdcUrl: 'http://127.0.0.1:9100', port: 8888, interval: 1000,
      scopeType: 'global', projectId: '', applicationName: ''
    })

    const scopeOptions = computed(() => {
      const options = new Map()
      resources.value.forEach((resource) => {
        if (resource.scopeId) options.set(resource.scopeId, { id: resource.scopeId, name: resource.scopeName || resource.scopeId })
      })
      projectStore.list.forEach((project) => {
        const id = `project:${project.id}`
        if (!options.has(id)) options.set(id, { id, name: `项目：${project.name}` })
      })
      return [...options.values()].sort((a, b) => a.name.localeCompare(b.name, 'zh-CN'))
    })

    const visibleResources = computed(() => resources.value.filter((resource) => {
      if (scopeFilter.value === 'all') return true
      if (scopeFilter.value === 'global') return !resource.scopeId
      return resource.scopeId === scopeFilter.value
    }))

    const wait = ms => new Promise(resolve => window.setTimeout(resolve, ms))

    async function loadResources() {
      if (loadingResources.value) return
      loadingResources.value = true
      let lastError = null
      try {
        for (let attempt = 0; attempt < 2; attempt += 1) {
          try {
            const r = await api.getCommunicationResources()
            resources.value = Array.isArray(r.data) ? r.data : []
            resourceLoadFailed.value = false
            resourceLoadError.value = ''
            return
          } catch (e) {
            lastError = e
            // Backend startup can overlap the first page visit. Retry only a
            // response-less network failure, never a server-side API error.
            if (attempt === 0 && !e?.response && e?.code !== 'ECONNABORTED') await wait(450)
            else break
          }
        }
        resources.value = []
        resourceLoadFailed.value = true
        resourceLoadError.value = formatConnectionError(lastError)
      } finally {
        loadingResources.value = false
      }
    }

    function resourceScopeLabel(resource) {
      return resource?.scopeName || (resource?.scopeId ? resource.scopeId : '公共资源')
    }

    function setFormScope(resource = null) {
      const scopeId = resource?.scopeId || ''
      const scopeName = resource?.scopeName || ''
      if (scopeId.startsWith('project:')) {
        form.scopeType = 'project'
        form.projectId = scopeId.slice('project:'.length)
        form.applicationName = ''
      } else if (scopeId.startsWith('application:')) {
        form.scopeType = 'application'
        form.projectId = ''
        form.applicationName = scopeName || scopeId.slice('application:'.length)
      } else {
        form.scopeType = 'global'
        form.projectId = ''
        form.applicationName = ''
      }
    }

    function scopePayload() {
      if (form.scopeType === 'project') {
        const project = projectStore.list.find(item => String(item.id) === String(form.projectId))
        if (!project) throw new Error('请选择所属项目')
        return { scopeId: `project:${project.id}`, scopeName: `项目：${project.name}` }
      }
      if (form.scopeType === 'application') {
        const name = String(form.applicationName || '').trim()
        if (!name) throw new Error('请填写应用名称')
        return { scopeId: `application:${name.toLowerCase()}`, scopeName: `应用：${name}` }
      }
      return { scopeId: '', scopeName: '' }
    }

    function openAddDialog() {
      editingIdx.value = -1
      form.name = ''; form.type = 'OPCUA'; form.endpoint = ''
      form.nodesText = ''; form.address = '0.0.0.0'; form.sdcUrl = 'http://127.0.0.1:9100'; form.port = 8888; form.interval = 1000
      setFormScope(projectStore.currentProject ? { scopeId: `project:${projectStore.currentProject.id}`, scopeName: projectStore.currentProject.name } : null)
      showDialog.value = true
    }

    function editResource(idx) {
      const r = resources.value[idx]
      editingIdx.value = idx
      form.name = r.name; form.type = r.type; form.endpoint = r.endpoint || ''
      form.address = r.address || '0.0.0.0'; form.sdcUrl = r.sdcUrl || 'http://127.0.0.1:9100'; form.port = r.port || 8888; form.interval = r.pollIntervalMs || 1000
      form.nodesText = (r.nodes || []).map(n => n.nodeId).join('\n')
      setFormScope(r)
      showDialog.value = true
    }

    function resourceIndexById(id) {
      return resources.value.findIndex(resource => resource.id === id)
    }

    function editResourceById(id) {
      const index = resourceIndexById(id)
      if (index >= 0) editResource(index)
    }

    function deleteResourceById(id) {
      const index = resourceIndexById(id)
      if (index >= 0) deleteResource(index)
    }

    function addNodeToResourceById(id) {
      const index = resourceIndexById(id)
      if (index >= 0) addNodeToResource(index)
    }

    function removeNodeFromResourceById(id, nodeIndex) {
      const index = resourceIndexById(id)
      if (index >= 0) removeNodeFromResource(index, nodeIndex)
    }

    function nameFromNodeId(nodeId) {
      return nodeId
        .replace(/^ns=\d+;(?:[isgb]=)?/, '')
        .replace(/^nsu=[^;]+;(?:[isgb]=)?/, '')
    }

    async function saveResource() {
      if (savingResource.value) return
      savingResource.value = true
      const nodes = []
      form.nodesText.split('\n').filter(Boolean).forEach(line => {
        const nid = line.trim()
        const name = nameFromNodeId(nid) || nid
        nodes.push({ name, nodeId: nid, browsePath: '\\Root\\Objects\\' + name })
      })

      const existing = editingIdx.value >= 0 ? resources.value[editingIdx.value] : null
      let scope
      try {
        scope = scopePayload()
      } catch (error) {
        savingResource.value = false
        showOperationMsg(false, error.message || '资源范围无效')
        return
      }
      const data = {
        id: existing?.id || 'res-' + Date.now(),
        name: form.name || (form.type === 'OPCUA' ? 'OPC UA' : form.type),
        type: form.type,
        endpoint: form.endpoint,
        address: form.address, sdcUrl: form.sdcUrl, port: form.port,
        pollIntervalMs: form.interval,
        enabled: existing?.enabled || false,
        ...scope,
        nodes
      }

      try {
        await api.saveCommunicationResource(data)
        await loadResources()
        showDialog.value = false
        showOperationMsg(true, existing ? '通信资源已更新' : '通信资源已保存')
      } catch (e) {
        showOperationMsg(false, '保存失败：' + (e?.message || e))
      } finally {
        savingResource.value = false
      }
    }

    function deleteResource(idx) {
      deleteTargetIdx.value = idx
    }

    async function confirmDeleteResource() {
      const idx = deleteTargetIdx.value
      if (idx < 0 || idx >= resources.value.length) return
      const r = resources.value[idx]
      try {
        if (r.running) {
          await api.stopCommunicationResource(r.id).catch(() => {})
        }
        await api.deleteCommunicationResource(r.id)
        await loadResources()
      } catch (e) {
        showOperationMsg(false, '删除失败：' + (e?.message || e))
      } finally {
        deleteTargetIdx.value = -1
      }
    }

    function addNodeToResource(idx) {
      nodeTargetIdx.value = idx
      nodeForm.name = ''
      nodeForm.nodeId = ''
      showNodeDialog.value = true
    }

    async function confirmAddNode() {
      if (!nodeForm.name.trim() && !nodeForm.nodeId.trim()) return
      const target = resources.value[nodeTargetIdx.value]
      if (!target) return
      const name = nodeForm.name.trim() || nameFromNodeId(nodeForm.nodeId.trim())
      const nodeId = nodeForm.nodeId.trim() || 'ns=1;s=' + name
      if (!Array.isArray(target.nodes)) target.nodes = []
      target.nodes.push({ name, nodeId, browsePath: '\\Root\\Objects\\' + name })
      try {
        await api.saveCommunicationResource(target)
        await loadResources()
        showNodeDialog.value = false
      } catch (e) {
        showOperationMsg(false, '添加节点失败：' + (e?.message || e))
      }
    }

    async function removeNodeFromResource(rIdx, nIdx) {
      const r = resources.value[rIdx]
      if (!r || !Array.isArray(r.nodes)) return
      r.nodes.splice(nIdx, 1)
      try {
        await api.saveCommunicationResource(r)
        await loadResources()
      } catch (e) {
        showOperationMsg(false, '移除节点失败：' + (e?.message || e))
      }
    }

    async function togglePoll(res) {
      if (!res?.id || resourceBusyId.value) return
      resourceBusyId.value = res.id
      try {
        if (res.running) {
          await api.stopCommunicationResource(res.id)
          showOperationMsg(true, `已停止「${res.name}」`)
        } else {
          const r = await api.startCommunicationResource(res.id)
          if (!r.data || r.data.started === false) {
            showOperationMsg(false, `启动「${res.name}」失败`)
          } else {
            showOperationMsg(true, `已启动「${res.name}」`)
          }
        }
        await loadResources()
        store.loadVariables()
      } catch (e) {
        showOperationMsg(false, (res.running ? '停止' : '启动') + '失败：' + formatConnectionError(e))
      } finally {
        resourceBusyId.value = ''
      }
    }

    function showOperationMsg(ok, text) {
      operationMsg.value = { ok, text }
      window.setTimeout(() => { operationMsg.value = null }, 4200)
    }

    async function testConn() {
      testLoading.value = true
      testMsg.value = null
      try {
        const r = form.type === 'SDC' ? await api.testSdc(form.sdcUrl) : await api.connectOpcua(form.endpoint)
        const countText = form.type === 'SDC' && r.data?.connected ? `，识别 ${r.data.variableCount || 0} 个可采集变量` : ''
        testMsg.value = { ok: r.data.connected, text: r.data.connected ? '✅ 连接成功' + countText : '❌ ' + r.data.message }
      } catch (e) {
        testMsg.value = { ok: false, text: '❌ ' + formatConnectionError(e) }
      } finally { testLoading.value = false }
    }

    function formatConnectionError(e) {
      const msg = e?.response?.data?.message || e?.message || ''
      if (!e?.response && (e?.code === 'ERR_NETWORK' || msg.toLowerCase() === 'network error')) {
        const target = window.location.port === '5170' ? 'http://127.0.0.1:8081' : window.location.origin
        return `无法连接后端服务（${target}）。请确认 MonitoringRuntime 已启动；开发模式下前端和后端需同时启动。`
      }
      if (e?.code === 'ECONNABORTED' || msg.includes('timeout')) {
        return '连接超时：端点不可达。若发布端在本机请用 opc.tcp://127.0.0.1:4840；若在远程电脑，请确认服务监听远程 IP/0.0.0.0 且防火墙放行端口。'
      }
      if (msg) return msg
      return '连接失败：请检查端点 URL、发布端监听地址和防火墙。'
    }

    function formatRealtimeValue(value) {
      if (value === null || value === undefined || value === '') return ''
      const num = Number(value)
      return Number.isFinite(num) ? num.toFixed(2) : String(value)
    }

    function getNodeValue(name) {
      if (!name) return null
      const v = Object.values(store.realtimeData).find(dp => dp.varName === name)
      return v ? formatRealtimeValue(v.value) : null
    }

    function getNodeQuality(name) {
      if (!name) return 'bad'
      const v = Object.values(store.realtimeData).find(dp => dp.varName === name)
      return v?.quality === 'GOOD' ? 'good' : 'bad'
    }

    onMounted(() => {
      loadResources()
      store.loadVariables()
      projectStore.fetchProjects().catch(() => {})
    })

    return {
      resources, visibleResources, scopeFilter, scopeOptions, projectStore, showDialog, editingIdx, form, testLoading, testMsg,
      showNodeDialog, nodeTargetIdx, nodeForm, deleteTargetIdx, operationMsg,
      resourceBusyId, savingResource,
      loadingResources, resourceLoadFailed, resourceLoadError,
      openAddDialog, editResourceById, saveResource, deleteResourceById,
      confirmDeleteResource, addNodeToResource, confirmAddNode, removeNodeFromResource,
      addNodeToResourceById, removeNodeFromResourceById, togglePoll, testConn, getNodeValue, getNodeQuality,
      resourceScopeLabel
    }
  }
}
</script>

<style scoped>
.comm-view { padding: 24px 28px; height: calc(100vh - var(--header-height) - var(--statusbar-height)); overflow-y: auto; background: var(--bg-secondary); }
.cv-header { max-width: var(--content-max-width); margin: 0 auto 20px; }
.cv-header h2 { font-size: var(--fs-2xl); line-height: var(--lh-tight); font-weight: var(--fw-bold); color: var(--text-primary); }
.cv-header-desc { font-size: var(--fs-sm); color: var(--text-muted); margin-top: 6px; }
.cv-operation-msg { margin-top: 12px; padding: 10px 12px; border-radius: var(--radius-md); font-size: var(--fs-xs); font-weight: var(--fw-semibold); }
.cv-operation-msg.ok { background: var(--success-bg); color: var(--success-text); border: 1px solid var(--success-border); }
.cv-operation-msg.err { background: var(--danger-bg); color: var(--danger-text); border: 1px solid var(--danger-border); }
.cv-scope-filter { max-width: var(--content-max-width); margin: 0 auto 14px; display: flex; align-items: center; flex-wrap: wrap; gap: 8px; padding: 10px 12px; border: 1px solid var(--border-light); border-radius: var(--radius-md); background: var(--bg-primary); box-shadow: var(--shadow-xs); }
.cv-scope-filter label { color: var(--text-secondary); font-size: var(--fs-sm); font-weight: var(--fw-bold); }
.cv-scope-filter select { min-width: 220px; min-height: 34px; }
.cv-scope-filter span { color: var(--text-tertiary); font-size: var(--fs-sm); }

.cv-list { max-width: var(--content-max-width); margin: 0 auto 20px; display: flex; flex-direction: column; gap: 14px; }
.cv-card { border: 1px solid var(--border-light); border-radius: var(--radius-lg); overflow: hidden; background: var(--bg-card); box-shadow: var(--shadow-card); transition: box-shadow var(--transition-fast), border-color var(--transition-fast), transform var(--transition-fast); }
.cv-card:hover { box-shadow: var(--shadow-md); border-color: var(--border-color); }
.cv-card.type-OPCUA { border-left: 3px solid var(--accent); }
.cv-card.type-UDP { border-left: 3px solid var(--color-warning); }
.cv-card.type-SDC { border-left: 3px solid var(--color-online); }

.cv-card-header { display: flex; justify-content: space-between; align-items: center; gap: 16px; padding: 14px 16px; background: var(--surface-muted); border-bottom: 1px solid var(--border-light); }
.cv-card-title { display: flex; align-items: center; gap: 10px; }
.cv-type-badge { font-size: 10px; font-weight: var(--fw-bold); padding: 4px 9px; border-radius: var(--radius-full); text-transform: uppercase; border: 1px solid transparent; }
.cv-type-badge.OPCUA { background: var(--info-bg); color: var(--info-text); border-color: var(--info-border); }
.cv-type-badge.UDP { background: var(--warning-bg); color: var(--warning-text); border-color: var(--warning-border); }
.cv-type-badge.SDC { background: var(--success-bg); color: var(--success-text); border-color: var(--success-border); }
.cv-name { font-size: var(--fs-lg); font-weight: var(--fw-bold); color: var(--text-primary); }
.cv-scope-tag { display: inline-flex; align-items: center; min-height: 23px; padding: 0 8px; border-radius: var(--radius-full); background: var(--accent-soft); border: 1px solid var(--info-border); color: var(--accent); font-size: var(--fs-xs); font-weight: var(--fw-semibold); }
.cv-card-actions { display: flex; align-items: center; gap: 8px; }
.cv-status { display: flex; align-items: center; gap: 6px; min-height: 26px; padding: 0 9px; border-radius: var(--radius-full); border: 1px solid var(--border-light); background: var(--bg-primary); font-size: var(--fs-xs); font-weight: var(--fw-semibold); color: var(--text-placeholder); }
.cv-status-dot { width: 7px; height: 7px; border-radius: 50%; background: var(--color-offline); }
.cv-status.running { color: var(--success-text); background: var(--success-bg); border-color: var(--success-border); }
.cv-status.running .cv-status-dot { background: var(--color-online); box-shadow: 0 0 0 3px rgba(47,143,99,0.12); }

.cv-card-body { padding: 16px; }
.cv-detail-row { display: flex; align-items: center; gap: 10px; margin-bottom: 12px; font-size: var(--fs-sm); }
.cv-detail-label { color: var(--text-tertiary); min-width: 76px; font-weight: var(--fw-semibold); }
.cv-detail-val { font-family: var(--font-mono); font-size: var(--fs-xs); color: var(--text-secondary); background: var(--surface-muted); padding: 5px 8px; border-radius: var(--radius-sm); border: 1px solid var(--border-light); }
.cv-sdc-hint { margin: 2px 0 0; font-size: var(--fs-xs); line-height: 1.6; color: var(--text-tertiary); }

.cv-nodes-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 10px; margin-top: 14px; padding-top: 14px; border-top: 1px solid var(--border-light); }
.cv-nodes-title { font-size: var(--fs-sm); font-weight: var(--fw-semibold); color: var(--text-secondary); }
.cv-nodes-list { display: flex; flex-direction: column; gap: 6px; }
.cv-node-row { display: flex; align-items: center; gap: 10px; min-height: 42px; padding: 7px 9px; border-radius: var(--radius-md); background: var(--surface-muted); border: 1px solid var(--border-light); font-size: var(--fs-xs); transition: background var(--transition-fast), border-color var(--transition-fast); }
.cv-node-row:hover { background: var(--bg-hover); }
.cv-node-idx { color: var(--text-placeholder); width: 18px; text-align: center; }
.cv-node-info { flex: 1; display: flex; flex-direction: column; gap: 2px; }
.cv-node-name { font-weight: var(--fw-medium); color: var(--text-primary); }
.cv-node-nid { font-family: var(--font-mono); font-size: 10px; color: var(--text-tertiary); }
.cv-node-value { display: flex; align-items: center; gap: 5px; }
.cv-nv-val { font-family: var(--font-mono); font-size: var(--fs-sm); font-weight: var(--fw-semibold); color: var(--text-primary); }
.cv-nodes-empty { text-align: center; padding: 18px 0; color: var(--text-placeholder); font-size: var(--fs-sm); }
.cv-actions { max-width: var(--content-max-width); margin: 8px auto 0; }
.cv-empty { text-align: center; padding: 60px 0; color: var(--text-placeholder); background: var(--bg-primary); border: 1px dashed var(--border-color); border-radius: var(--radius-lg); }
.cv-empty-icon { font-size: 48px; margin-bottom: 12px; }
.cv-empty-hint { font-size: var(--fs-sm); margin-top: 6px; }

.node-preview {
  padding: 10px 12px;
  border-radius: var(--radius-md);
  background: var(--surface-muted);
  border: 1px solid var(--border-light);
  color: var(--text-tertiary);
}
.comm-connect-bar { display: flex; align-items: center; gap: 10px; margin: 8px 0 16px; }
.test-msg { font-size: var(--fs-xs); font-weight: var(--fw-semibold); }
.test-msg.ok { color: var(--success-text); }
.test-msg.err { color: var(--danger-text); }

@media (max-width: 760px) {
  .comm-view { padding: 16px 12px; }
  .cv-card-header { align-items: flex-start; flex-direction: column; }
  .cv-card-actions { width: 100%; flex-wrap: wrap; }
  .cv-detail-row { align-items: flex-start; flex-direction: column; gap: 4px; }
  .cv-detail-label { min-width: 0; }
  .comm-connect-bar { align-items: stretch; flex-direction: column; }
}
</style>
