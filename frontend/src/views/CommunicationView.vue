<template>
  <div class="comm-view">
    <div class="cv-header">
      <h2>通信资源</h2>
      <p class="cv-header-desc">配置 OPC UA / UDP 数据源，系统将从远程节点读取数据并自动创建变量</p>
      <div v-if="operationMsg" class="cv-operation-msg" :class="operationMsg.ok ? 'ok' : 'err'">{{ operationMsg.text }}</div>
    </div>

    <!-- 资源列表 -->
    <div class="cv-list">
      <div v-for="(res, idx) in resources" :key="res.id" class="cv-card" :class="'type-' + res.type">
        <div class="cv-card-header">
          <div class="cv-card-title">
            <span class="cv-type-badge" :class="res.type">{{ res.type === 'OPCUA' ? 'OPC UA' : res.type }}</span>
            <span class="cv-name">{{ res.name }}</span>
          </div>
          <div class="cv-card-actions">
            <span class="cv-status" :class="res.enabled ? 'running' : ''">
              <span class="cv-status-dot"></span>
              {{ res.enabled ? '采集中' : '已停止' }}
            </span>
            <button v-if="res.type === 'OPCUA' || res.type === 'UDP'" class="btn btn-sm" :class="res.enabled ? 'btn-danger' : 'btn-primary'"
              @click="togglePoll(res, idx)">
              {{ res.enabled ? '停止' : '启动采集' }}
            </button>
            <button class="btn btn-sm" @click="editResource(idx)">编辑</button>
            <button class="btn btn-sm" @click="deleteResource(idx)">删除</button>
          </div>
        </div>
        <div class="cv-card-body">

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
              <button class="btn btn-xs" @click="addNodeToResource(idx)">＋ 添加节点</button>
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
                <button class="btn btn-sm" @click="removeNodeFromResource(idx, ni)" title="移除">✕</button>
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

        </div>
      </div>

      <!-- 空状态 -->
      <div v-if="resources.length === 0" class="cv-empty">
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
          </select>
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

        <div class="form-group">
          <label>采集间隔 (ms)</label>
          <input type="number" v-model.number="form.interval" placeholder="1000" />
        </div>

        <div class="dialog-actions">
          <button class="btn btn-secondary" @click="showDialog = false">取消</button>
          <button class="btn btn-primary" @click="saveResource">保存</button>
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
import { ref, reactive, onMounted } from 'vue'
import { useMonitorStore } from '../store/index.js'
import api from '../api/index.js'
import ConfirmDialog from '../components/common/ConfirmDialog.vue'

const STORAGE_KEY = 'monitor-comm-resources'

export default {
  name: 'CommunicationView',
  components: { ConfirmDialog },
  setup() {
    const store = useMonitorStore()
    const resources = ref([])
    const showDialog = ref(false)
    const editingIdx = ref(-1)
    const testLoading = ref(false)
    const testMsg = ref(null)
    const showNodeDialog = ref(false)
    const nodeTargetIdx = ref(-1)
    const nodeForm = reactive({ name: '', nodeId: '' })
    const deleteTargetIdx = ref(-1)
    const operationMsg = ref(null)

    const form = reactive({
      name: '', type: 'OPCUA', endpoint: '', nodesText: '', address: '0.0.0.0', port: 8888, interval: 1000
    })

    function loadResources() {
      try {
        const s = localStorage.getItem(STORAGE_KEY)
        const loaded = s ? JSON.parse(s) : []
        resources.value = Array.isArray(loaded)
          ? loaded.filter(item => item?.type === 'OPCUA' || item?.type === 'UDP')
          : []
        if (Array.isArray(loaded) && loaded.length !== resources.value.length) saveResources()
      } catch { resources.value = [] }
    }

    function saveResources() {
      localStorage.setItem(STORAGE_KEY, JSON.stringify(resources.value))
    }

    function openAddDialog() {
      editingIdx.value = -1
      form.name = ''; form.type = 'OPCUA'; form.endpoint = ''
      form.nodesText = ''; form.address = '0.0.0.0'; form.port = 8888; form.interval = 1000
      showDialog.value = true
    }

    function editResource(idx) {
      const r = resources.value[idx]
      editingIdx.value = idx
      form.name = r.name; form.type = r.type; form.endpoint = r.endpoint || ''
      form.address = r.address || '0.0.0.0'; form.port = r.port || 8888; form.interval = r.pollIntervalMs || 1000
      form.nodesText = (r.nodes || []).map(n => n.nodeId).join('\n')
      showDialog.value = true
    }

    function nameFromNodeId(nodeId) {
      return nodeId
        .replace(/^ns=\d+;(?:[isgb]=)?/, '')
        .replace(/^nsu=[^;]+;(?:[isgb]=)?/, '')
    }

    function saveResource() {
      const nodes = []
      form.nodesText.split('\n').filter(Boolean).forEach(line => {
        const nid = line.trim()
        const name = nameFromNodeId(nid) || nid
        nodes.push({ name, nodeId: nid, browsePath: '\\Root\\Objects\\' + name })
      })

      const data = {
        id: 'res-' + Date.now(),
        name: form.name || (form.type === 'OPCUA' ? 'OPC UA' : form.type),
        type: form.type,
        endpoint: form.endpoint,
        address: form.address, port: form.port,
        pollIntervalMs: form.interval,
        enabled: false,
        nodes
      }

      if (editingIdx.value >= 0) {
        data.enabled = resources.value[editingIdx.value].enabled
        resources.value[editingIdx.value] = data
      } else {
        resources.value.push(data)
      }
      saveResources()
      showDialog.value = false
    }

    function deleteResource(idx) {
      deleteTargetIdx.value = idx
    }

    async function confirmDeleteResource() {
      const idx = deleteTargetIdx.value
      if (idx < 0 || idx >= resources.value.length) return
      const r = resources.value[idx]
      if (r.enabled) {
          if (r.type === 'OPCUA') {
            await api.post('/opcua/poll/stop', {}).catch(() => {})
          } else if (r.type === 'UDP') {
            await api.udpStop().catch(() => {})
          }
      }
      resources.value.splice(idx, 1)
      deleteTargetIdx.value = -1
      saveResources()
    }

    function addNodeToResource(idx) {
      nodeTargetIdx.value = idx
      nodeForm.name = ''
      nodeForm.nodeId = ''
      showNodeDialog.value = true
    }

    function confirmAddNode() {
      if (!nodeForm.name.trim() && !nodeForm.nodeId.trim()) return
      const name = nodeForm.name.trim() || nameFromNodeId(nodeForm.nodeId.trim())
      const nodeId = nodeForm.nodeId.trim() || 'ns=1;s=' + name
      if (!resources.value[nodeTargetIdx.value].nodes) {
        resources.value[nodeTargetIdx.value].nodes = []
      }
      resources.value[nodeTargetIdx.value].nodes.push({
        name, nodeId, browsePath: '\\Root\\Objects\\' + name
      })
      saveResources()
      showNodeDialog.value = false
    }function removeNodeFromResource(rIdx, nIdx) {
      resources.value[rIdx].nodes.splice(nIdx, 1)
      saveResources()
    }

    async function togglePoll(res, idx) {
      if (res.enabled) {
        if (res.type === 'OPCUA') {
          try { await api.post('/opcua/poll/stop', {}) } catch {}
        } else if (res.type === 'UDP') {
          try { await api.udpStop() } catch {}
        }
        res.enabled = false
        saveResources()
      } else {
        if (res.type === 'OPCUA') {
          if (!res.endpoint) return
          try {
            const payload = {
              endpoint: res.endpoint,
              interval: res.pollIntervalMs || 1000,
              nodes: (res.nodes || []).map(n => ({ nodeId: n.nodeId, name: n.name }))
            }
            const r = await api.post('/opcua/poll/start', payload)
            if (r.data.started) {
              res.enabled = true
              saveResources()
              store.loadVariables()
            }
          } catch (e) {
            showOperationMsg(false, '启动采集失败：' + formatConnectionError(e))
          }
        } else if (res.type === 'UDP') {
          try {
              const r = await api.udpStart(res.address || '0.0.0.0', res.port || 8888)
            if (r.data.started) {
              res.enabled = true
              saveResources()
              store.loadVariables()
            }
          } catch (e) {
            showOperationMsg(false, '启动 UDP 采集失败：' + (e.message || e))
          }
        }
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
        const r = await api.connectOpcua(form.endpoint)
        testMsg.value = { ok: r.data.connected, text: r.data.connected ? '✅ 连接成功' : '❌ ' + r.data.message }
      } catch (e) {
        testMsg.value = { ok: false, text: '❌ ' + formatConnectionError(e) }
      } finally { testLoading.value = false }
    }

    function formatConnectionError(e) {
      const msg = e?.response?.data?.message || e?.message || ''
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
    })

    return {
      resources, showDialog, editingIdx, form, testLoading, testMsg,
      showNodeDialog, nodeTargetIdx, nodeForm, deleteTargetIdx, operationMsg,
      openAddDialog, editResource, saveResource, deleteResource,
      confirmDeleteResource, addNodeToResource, confirmAddNode, removeNodeFromResource,
      togglePoll, testConn, getNodeValue, getNodeQuality
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

.cv-list { max-width: var(--content-max-width); margin: 0 auto 20px; display: flex; flex-direction: column; gap: 14px; }
.cv-card { border: 1px solid var(--border-light); border-radius: var(--radius-lg); overflow: hidden; background: var(--bg-card); box-shadow: var(--shadow-card); transition: box-shadow var(--transition-fast), border-color var(--transition-fast), transform var(--transition-fast); }
.cv-card:hover { box-shadow: var(--shadow-md); border-color: var(--border-color); }
.cv-card.type-OPCUA { border-left: 3px solid var(--accent); }
.cv-card.type-UDP { border-left: 3px solid var(--color-warning); }

.cv-card-header { display: flex; justify-content: space-between; align-items: center; gap: 16px; padding: 14px 16px; background: var(--surface-muted); border-bottom: 1px solid var(--border-light); }
.cv-card-title { display: flex; align-items: center; gap: 10px; }
.cv-type-badge { font-size: 10px; font-weight: var(--fw-bold); padding: 4px 9px; border-radius: var(--radius-full); text-transform: uppercase; border: 1px solid transparent; }
.cv-type-badge.OPCUA { background: var(--info-bg); color: var(--info-text); border-color: var(--info-border); }
.cv-type-badge.UDP { background: var(--warning-bg); color: var(--warning-text); border-color: var(--warning-border); }
.cv-name { font-size: var(--fs-lg); font-weight: var(--fw-bold); color: var(--text-primary); }
.cv-card-actions { display: flex; align-items: center; gap: 8px; }
.cv-status { display: flex; align-items: center; gap: 6px; min-height: 26px; padding: 0 9px; border-radius: var(--radius-full); border: 1px solid var(--border-light); background: var(--bg-primary); font-size: var(--fs-xs); font-weight: var(--fw-semibold); color: var(--text-placeholder); }
.cv-status-dot { width: 7px; height: 7px; border-radius: 50%; background: var(--color-offline); }
.cv-status.running { color: var(--success-text); background: var(--success-bg); border-color: var(--success-border); }
.cv-status.running .cv-status-dot { background: var(--color-online); box-shadow: 0 0 0 3px rgba(47,143,99,0.12); }

.cv-card-body { padding: 16px; }
.cv-detail-row { display: flex; align-items: center; gap: 10px; margin-bottom: 12px; font-size: var(--fs-sm); }
.cv-detail-label { color: var(--text-tertiary); min-width: 76px; font-weight: var(--fw-semibold); }
.cv-detail-val { font-family: var(--font-mono); font-size: var(--fs-xs); color: var(--text-secondary); background: var(--surface-muted); padding: 5px 8px; border-radius: var(--radius-sm); border: 1px solid var(--border-light); }

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
