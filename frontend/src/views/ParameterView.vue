<template>
  <div class="parameter-view">
    <header class="pv-header">
      <div>
        <h2>参数在线下发</h2>
        <p>由已启用通信资源采集到的变量自动生成；不需要重复填写通信地址。</p>
      </div>
      <div class="pv-header-actions">
        <button class="btn btn-primary" :disabled="loading || deletingNodes" @click="loadAll">{{ loading ? '同步中…' : '同步通信资源' }}</button>
        <button class="btn btn-danger" :disabled="!offlineParameters.length || deletingNodes" @click="openBulkDeleteDialog('offline')">
          删除离线节点{{ offlineParameters.length ? ` (${offlineParameters.length})` : '' }}
        </button>
        <button class="btn btn-danger" :disabled="!filteredParameters.length || deletingNodes" @click="openBulkDeleteDialog('all')">
          删除全部节点{{ filteredParameters.length ? ` (${filteredParameters.length})` : '' }}
        </button>
      </div>
    </header>

    <div v-if="message.text" class="pv-message" :class="message.ok ? 'ok' : 'error'">{{ message.text }}</div>

    <section class="pv-notice">
      <strong>下发规则：</strong>OPC UA、Modbus TCP、UDP、软件定义通信均复用变量所属通信资源。Modbus 的输入寄存器、离散输入区是只读的；UDP 需要在通信资源中设置“下发控制器 IP / 端口”。
    </section>

    <div class="pv-filter">
      <label>项目/资源范围</label>
      <select v-model="selectedScope">
        <option value="all">全部可见资源</option>
        <option value="global">公共资源</option>
        <option v-for="scope in scopeOptions" :key="scope.id" :value="scope.id">{{ scope.name }}</option>
      </select>
      <span>{{ filteredParameters.length }} 个自动下发资源</span>
    </div>

    <section v-if="filteredParameters.length" class="pv-grid">
      <article v-for="parameter in filteredParameters" :key="parameter.id" class="pv-card">
        <div class="pv-card-title">
          <strong>{{ parameter.name }}</strong>
          <span class="pv-protocol" :class="String(parameter.source).toLowerCase()">{{ protocolLabel(parameter.source) }}</span>
        </div>
        <div class="pv-meta"><span>通信资源</span><code>{{ parameter.resourceName || resourceName(parameter.resourceId) || '-' }}</code></div>
        <div class="pv-meta"><span>通信地址</span><code>{{ parameter.opcuaNodeId || '-' }}</code></div>
        <div class="pv-meta"><span>数据质量</span><b :class="String(parameter.quality || '').toLowerCase()">{{ parameter.quality || '未知' }}</b></div>
        <div class="pv-current">当前值 <strong>{{ formatValue(parameter.value) }}</strong><span v-if="parameter.unit">{{ parameter.unit }}</span></div>
        <div class="pv-command">
          <input v-model.number="draftValues[parameter.id]" type="number" step="any" :disabled="submittingId === parameter.id || Boolean(commandBlockReason(parameter))" />
          <button class="btn btn-primary pv-node-action" :disabled="submittingId === parameter.id || Boolean(commandBlockReason(parameter))" @click="requestWrite(parameter)">
            {{ submittingId === parameter.id ? '下发中…' : '确认下发' }}
          </button>
        </div>
        <div class="pv-node-actions">
          <button class="btn btn-danger pv-node-action" :disabled="deletingNodes || submittingId === parameter.id" @click="deleteTarget = parameter">删除此节点</button>
        </div>
        <small v-if="commandBlockReason(parameter)" class="pv-readonly">{{ commandBlockReason(parameter) }}</small>
      </article>
    </section>
    <section v-else class="pv-empty">
      <h3>暂无可下发资源</h3>
      <p>请先在“通信资源”中启用 OPC UA、Modbus TCP、UDP 或软件定义通信采集。采集到变量后，本页会自动生成对应的下发资源。</p>
    </section>

    <section class="pv-audit">
      <div class="pv-audit-header">
        <div>
          <h3>下发审计记录</h3>
          <p>保留目标值、回读/ACK、操作者、命令号和执行结果。</p>
        </div>
        <div class="pv-audit-actions">
          <button class="btn btn-sm btn-secondary" :disabled="auditLoading || auditClearing" @click="refreshAuditRecords">{{ auditLoading ? '刷新中…' : '刷新记录' }}</button>
          <button class="btn btn-sm btn-danger" :disabled="!commands.length || auditLoading || auditClearing" @click="clearAuditDialog = true">删除记录</button>
        </div>
      </div>
      <table>
        <thead><tr><th>时间</th><th>参数</th><th>协议</th><th>目标值</th><th>回读/ACK</th><th>结果</th><th>操作者</th><th>说明</th></tr></thead>
        <tbody>
          <tr v-for="item in commands" :key="item.id">
            <td>{{ item.time }}</td><td>{{ item.name }}</td><td>{{ protocolLabel(item.source) }}</td><td>{{ item.value }}</td>
            <td :class="readbackClass(item)">{{ readbackLabel(item) }}</td>
            <td :class="item.ok ? 'command-ok' : 'command-error'">{{ item.ok ? '成功' : '失败' }}</td>
            <td>{{ item.operator }}</td><td class="pv-audit-detail" :title="item.detail || item.message">{{ item.detail || item.message }}</td>
          </tr>
          <tr v-if="!commands.length"><td colspan="8" class="pv-no-logs">暂无下发审计记录</td></tr>
        </tbody>
      </table>
    </section>

    <ConfirmDialog
      v-if="pendingCommand"
      title="确认参数下发"
      :message="`确认向控制器下发“${pendingCommand.parameter.name}”的新值：${pendingCommand.value}？`"
      detail="系统将使用该变量采集时所属的通信资源和地址下发，不会使用手工填写的地址。"
      confirm-text="确认下发"
      @close="pendingCommand = null"
      @confirm="executeWrite"
    />
    <ConfirmDialog
      v-if="deleteTarget"
      title="删除数据节点"
      :message="`确认删除数据节点“${deleteTarget.name}”？`"
      detail="将删除该节点的变量定义、实时缓存与历史缓存；通信资源本身不会被删除。采集恢复后，该资源可重新自动创建节点。"
      confirm-text="确认删除"
      submitting-text="删除中…"
      :submitting="deletingNodes"
      danger
      @close="deleteTarget = null"
      @confirm="confirmDeleteNode"
    />
    <ConfirmDialog
      v-if="bulkDeleteDialog"
      :title="bulkDeleteDialog.title"
      :message="bulkDeleteDialog.message"
      :detail="bulkDeleteDialog.detail"
      confirm-text="确认删除"
      submitting-text="删除中…"
      :submitting="deletingNodes"
      danger
      @close="bulkDeleteDialog = null"
      @confirm="confirmBulkDeleteNodes"
    />
    <ConfirmDialog
      v-if="clearAuditDialog"
      title="删除下发审计记录"
      message="确认删除全部下发审计记录吗？"
      detail="此操作会清空运行包中已保存的下发审计记录，无法恢复。"
      confirm-text="确认删除"
      :submitting="auditClearing"
      danger
      @close="clearAuditDialog = false"
      @confirm="clearAuditRecords"
    />
  </div>
</template>

<script setup>
import { computed, reactive, ref, watch } from 'vue'
import api from '../api/index.js'
import { useMonitorStore } from '../store/index.js'
import { useProjectStore } from '../store/projectStore.js'
import ConfirmDialog from '../components/common/ConfirmDialog.vue'
import { writeCollectedVariable } from '../services/automaticParameterService.js'

const projectStore = useProjectStore()
const store = useMonitorStore()
const resources = computed(() => store.communicationResources)
const variables = computed(() => {
  const realtimeById = new Map(Object.values(store.realtimeData).map((point) => [String(point.varId), point]))
  return store.variables.map((variable) => {
    const latest = realtimeById.get(String(variable.id))
    return latest ? { ...variable, value: latest.value, quality: latest.quality, timestamp: latest.timestamp } : variable
  })
})
const loading = ref(false)
const submittingId = ref(null)
const selectedScope = ref('all')
const pendingCommand = ref(null)
const commands = computed(() => store.writeAuditRecords.map(normalizeAuditRecord).reverse())
const auditLoading = ref(false)
const auditClearing = ref(false)
const clearAuditDialog = ref(false)
const deleteTarget = ref(null)
const bulkDeleteDialog = ref(null)
const deletingNodes = ref(false)
const draftValues = reactive({})
const message = reactive({ ok: true, text: '' })
const collectedSources = new Set(['OPCUA', 'MODBUS', 'UDP', 'SDC'])

watch(variables, (items) => {
  items.forEach((variable) => {
    if (!(variable.id in draftValues)) draftValues[variable.id] = Number(variable.value) || 0
  })
}, { immediate: true })

const scopeOptions = computed(() => {
  const scopes = new Map()
  resources.value.forEach((resource) => {
    if (resource.scopeId) scopes.set(resource.scopeId, { id: resource.scopeId, name: resource.scopeName || resource.scopeId })
  })
  variables.value.forEach((variable) => {
    if (variable.scopeId && !scopes.has(variable.scopeId)) scopes.set(variable.scopeId, { id: variable.scopeId, name: variable.scopeName || variable.scopeId })
  })
  return [...scopes.values()].sort((left, right) => left.name.localeCompare(right.name, 'zh-CN'))
})

const filteredParameters = computed(() => variables.value.filter((variable) => {
  if (!collectedSources.has(String(variable.source || '').toUpperCase())) return false
  if (selectedScope.value === 'all') return true
  if (selectedScope.value === 'global') return !variable.scopeId
  return variable.scopeId === selectedScope.value
}))

function parameterIsOffline(parameter) {
  const point = store.realtimeData[parameter.id]
  if (!point) return true
  const source = String(parameter.source || '').toUpperCase()
  const resource = resources.value.find((item) => String(item.id) === String(parameter.resourceId))
  if (!resource || !resource.enabled) return true
  const interval = Math.min(600000, Math.max(50, Number(resource.pollIntervalMs) || 1000))
  const maxAge = Math.max(5000, interval * (source === 'UDP' ? 3 : 4) + 1000)
  const timestamp = Number(point.timestamp)
  if (!Number.isFinite(timestamp) || timestamp <= 0 || Date.now() - timestamp > maxAge) return true
  const quality = String(point.quality || '').toUpperCase()
  return quality === 'OFFLINE' || quality === 'DISCONNECTED'
}

const offlineParameters = computed(() => filteredParameters.value.filter(parameterIsOffline))

function protocolLabel(value) {
  return ({ OPCUA: 'OPC UA', MODBUS: 'Modbus TCP', UDP: 'UDP', SDC: '软件定义通信' })[String(value || '').toUpperCase()] || value || '-'
}

function resourceName(id) {
  return resources.value.find((resource) => String(resource.id) === String(id))?.name || ''
}

function formatValue(value) {
  const number = Number(value)
  return Number.isFinite(number) ? Number(number.toFixed(6)) : (value ?? '-')
}

function formatAuditTime(timestamp) {
  const value = Number(timestamp)
  if (!Number.isFinite(value) || value <= 0) return '-'
  return new Date(value).toLocaleString('zh-CN', { hour12: false })
}

function normalizeAuditRecord(record, index) {
  const hasReadbackValue = Boolean(record?.hasReadbackValue) && Number.isFinite(Number(record?.readbackValue))
  const writeStatus = String(record?.writeStatus || '')
  return {
    id: record?.id || `${record?.timestamp || 'audit'}-${index}`,
    time: formatAuditTime(record?.timestamp),
    name: String(record?.name || `变量 ${record?.varId ?? '-'}`),
    source: String(record?.source || ''),
    value: formatValue(record?.targetValue),
    ok: record?.ok === true || (record?.ok !== false && writeStatus === 'SUCCESS'),
    operator: String(record?.operator || '系统'),
    message: String(record?.message || writeStatus || '下发完成'),
    detail: String(record?.detail || record?.message || writeStatus || ''),
    readback: {
      status: String(record?.readbackStatus || 'NOT_EXECUTED'),
      value: record?.readbackValue,
      hasValue: hasReadbackValue,
      message: String(record?.readbackMessage || '')
    }
  }
}

function readbackLabel(command) {
  const readback = command.readback || {}
  const status = String(readback.status || 'NOT_EXECUTED').toUpperCase()
  const value = readback.hasValue ? `：${formatValue(readback.value)}` : ''
  if (status === 'MATCH') return `匹配${value}`
  if (status === 'MISMATCH') return `不匹配${value}`
  if (status === 'ACK_CONFIRMED') return '已收到 UDP ACK'
  if (status === 'NOT_SUPPORTED') return '不支持标准回读'
  if (status === 'LOCAL_VALUE') return `本地确认${value}`
  if (status === 'FAILED') return `回读失败${readback.message ? `：${readback.message}` : ''}`
  return '未执行'
}

function readbackClass(command) {
  const status = String(command.readback?.status || 'NOT_EXECUTED').toUpperCase()
  if (status === 'MATCH' || status === 'ACK_CONFIRMED' || status === 'LOCAL_VALUE') return 'readback-match'
  if (status === 'MISMATCH' || status === 'FAILED') return 'readback-error'
  return 'readback-neutral'
}

function isReadOnly(variable) {
  const binding = String(variable.opcuaNodeId || '').toLowerCase()
  return String(variable.source).toUpperCase() === 'MODBUS' && (binding.startsWith('modbus-json:') || binding.includes('input_register') || binding.includes('discrete_input'))
}

function commandBlockReason(variable) {
  if (String(variable.source).toUpperCase() === 'MODBUS' && String(variable.opcuaNodeId || '').toLowerCase().startsWith('modbus-json:')) {
    return '该变量从 Modbus 寄存器中的 JSON 文本解析而来，只支持读取。'
  }
  if (isReadOnly(variable)) return '该 Modbus 点位位于只读输入区；请绑定 Coil 或 Holding Register 后再下发。'
  const source = String(variable.source || '').toUpperCase()
  const resource = resources.value.find((item) => String(item.id) === String(variable.resourceId))
  if (!resource) return '变量所属的通信资源已不存在；请同步或重新创建资源。'
  if (String(resource.type || '').toUpperCase() !== source) return '变量所属资源的通信类型已变更，请重新采集后再下发。'
  const binding = String(variable.opcuaNodeId || '')
  if (!binding) return '变量缺少通信地址绑定，无法下发。'
  if (source === 'OPCUA' && binding.includes('#')) return '该变量来自 OPC UA JSON 字符串的拆分字段，不能单独写入。'
  if (source === 'UDP' && (!resource.udpCommandHost || !Number(resource.udpCommandPort))) return '请先在通信资源中填写 UDP 下发控制器 IP 和端口。'
  return ''
}

async function loadAll() {
  loading.value = true
  message.text = ''
  try {
    await Promise.all([store.refreshResidentMetadata(), store.loadRealtime()])
    const currentId = projectStore.currentProject?.id
    const currentScope = currentId == null ? '' : `project:${currentId}`
    if (selectedScope.value === 'all' && currentScope && scopeOptions.value.some((scope) => scope.id === currentScope)) selectedScope.value = currentScope
  } catch (error) {
    message.ok = false
    message.text = `同步通信资源失败：${error?.message || '后端不可用'}`
  } finally {
    loading.value = false
  }
}

async function refreshAuditRecords() {
  if (auditLoading.value) return
  auditLoading.value = true
  try {
    const loaded = await store.loadWriteAudit()
    if (!loaded) throw new Error('后端不可用')
  } catch (error) {
    message.ok = false
    message.text = `刷新下发审计记录失败：${error?.message || '后端不可用'}`
  } finally {
    auditLoading.value = false
  }
}

async function clearAuditRecords() {
  if (auditClearing.value) return
  auditClearing.value = true
  try {
    await api.clearWriteAudit()
    await store.loadWriteAudit()
    clearAuditDialog.value = false
    message.ok = true
    message.text = '下发审计记录已删除'
  } catch (error) {
    message.ok = false
    message.text = `删除下发审计记录失败：${error?.message || '后端不可用'}`
  } finally {
    auditClearing.value = false
  }
}

async function refreshAfterNodeDelete() {
  await Promise.all([
    store.refreshResidentMetadata(),
    store.loadRealtime(),
    store.loadHistoryWide()
  ])
}

function clearDeletedDraftValues(ids) {
  ids.forEach((id) => { delete draftValues[id] })
}

function openBulkDeleteDialog(mode) {
  const targets = mode === 'offline' ? offlineParameters.value : filteredParameters.value
  if (!targets.length || deletingNodes.value) return
  const count = targets.length
  bulkDeleteDialog.value = {
    ids: targets.map((parameter) => parameter.id),
    title: mode === 'offline' ? '删除离线数据节点' : '删除全部数据节点',
    message: mode === 'offline'
      ? `确认删除当前范围内的 ${count} 个离线数据节点？`
      : `确认删除当前范围内的全部 ${count} 个数据节点？`,
    detail: '仅删除当前筛选范围内由通信资源自动采集的节点；通信资源本身不会被删除。删除后，恢复采集的资源会自动重新创建对应节点。'
  }
}

async function confirmDeleteNode() {
  const target = deleteTarget.value
  if (!target || deletingNodes.value) return
  deletingNodes.value = true
  try {
    await api.deleteVariable(target.id)
    clearDeletedDraftValues([target.id])
    deleteTarget.value = null
    await refreshAfterNodeDelete()
    message.ok = true
    message.text = `数据节点“${target.name}”已删除`
  } catch (error) {
    message.ok = false
    message.text = `删除数据节点失败：${error?.message || '后端不可用'}`
  } finally {
    deletingNodes.value = false
  }
}

async function confirmBulkDeleteNodes() {
  const dialog = bulkDeleteDialog.value
  if (!dialog?.ids?.length || deletingNodes.value) return
  deletingNodes.value = true
  try {
    const response = await api.bulkDeleteVariables('selected', dialog.ids)
    const deletedIds = Array.isArray(response.data?.deletedIds) ? response.data.deletedIds : dialog.ids
    clearDeletedDraftValues(deletedIds)
    bulkDeleteDialog.value = null
    await refreshAfterNodeDelete()
    message.ok = true
    message.text = `已删除 ${deletedIds.length} 个数据节点`
  } catch (error) {
    message.ok = false
    message.text = `批量删除数据节点失败：${error?.message || '后端不可用'}`
  } finally {
    deletingNodes.value = false
  }
}

function requestWrite(parameter) {
  const blockReason = commandBlockReason(parameter)
  if (blockReason) {
    message.ok = false
    message.text = blockReason
    return
  }
  const value = Number(draftValues[parameter.id])
  if (!Number.isFinite(value)) {
    message.ok = false
    message.text = '请输入有效的有限数值后再下发。'
    return
  }
  pendingCommand.value = { parameter, value }
}

async function executeWrite() {
  const command = pendingCommand.value
  if (!command || submittingId.value != null) return
  pendingCommand.value = null
  submittingId.value = command.parameter.id
  try {
    const response = await writeCollectedVariable(command.parameter.id, command.value)
    const payload = response?.data || {}
    const readback = {
      status: String(payload.readbackStatus || 'NOT_EXECUTED'),
      value: payload.readbackValue,
      hasValue: payload.readbackValue !== null && payload.readbackValue !== undefined && Number.isFinite(Number(payload.readbackValue)),
      message: String(payload.readbackMessage || '')
    }
    if (readback.hasValue) command.parameter.value = Number(payload.readbackValue)
    await loadAll()
    await refreshAuditRecords()
    message.ok = true
    message.text = `“${command.parameter.name}”已通过 ${protocolLabel(command.parameter.source)} 下发；${readbackLabel({ readback })}。`
  } catch (error) {
    await refreshAuditRecords()
    message.ok = false
    message.text = `下发失败：${error?.message || '控制器未确认写入'}`
  } finally {
    submittingId.value = null
  }
}

</script>

<style scoped>
.parameter-view { height: calc(100vh - var(--header-height) - var(--statusbar-height)); overflow: auto; padding: 24px 28px 42px; background: var(--bg-secondary); color: var(--text-primary); }
.pv-header, .pv-notice, .pv-filter, .pv-grid, .pv-empty, .pv-audit, .pv-message { max-width: 1320px; margin-left: auto; margin-right: auto; }
.pv-header { display: flex; justify-content: space-between; align-items: flex-start; gap: 16px; margin-bottom: 16px; }.pv-header h2 { margin: 0 0 6px; font-size: var(--fs-2xl); }.pv-header p { margin: 0; color: var(--text-muted); }.pv-header-actions { display: flex; flex-wrap: wrap; justify-content: flex-end; gap: 8px; }.pv-notice, .pv-filter, .pv-audit, .pv-empty { border: 1px solid var(--border-light); border-radius: var(--radius-md); background: var(--bg-card); }.pv-notice { padding: 12px 14px; line-height: 1.6; color: var(--text-secondary); }.pv-filter { display: flex; align-items: center; gap: 10px; margin-top: 14px; padding: 12px 14px; }.pv-filter span { color: var(--text-muted); font-size: var(--fs-sm); }.pv-filter select { min-width: 230px; }.pv-message { margin-bottom: 12px; padding: 10px 12px; border: 1px solid var(--success-border); border-radius: var(--radius-md); background: var(--success-bg); color: var(--success-text); }.pv-message.error { border-color: var(--danger-border); background: var(--danger-bg); color: var(--danger-text); }.pv-grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(300px, 1fr)); gap: 14px; margin-top: 14px; }.pv-card { padding: 15px; border: 1px solid var(--border-light); border-radius: var(--radius-md); background: var(--bg-card); box-shadow: var(--shadow-xs); }.pv-card-title { display: flex; gap: 8px; align-items: center; justify-content: space-between; margin-bottom: 12px; }.pv-protocol { padding: 3px 8px; border-radius: 999px; background: var(--surface-muted); color: var(--text-secondary); font-family: var(--font-family); font-size: 11px; }.pv-protocol.opcua { background: #e7f0ff; color: #245bab; }.pv-protocol.modbus { background: #f7edda; color: #8d5b12; }.pv-protocol.udp { background: #efe7ff; color: #6a43a3; }.pv-protocol.sdc { background: #dff4ec; color: #1e7855; }.pv-meta { display: grid; grid-template-columns: 76px minmax(0, 1fr); gap: 8px; margin: 7px 0; font-size: var(--fs-sm); }.pv-meta > span { color: var(--text-muted); }.pv-meta code { overflow: hidden; text-overflow: ellipsis; white-space: nowrap; font-family: var(--font-family); }.pv-meta b.good { color: var(--success-text); }.pv-meta b.bad { color: var(--danger-text); }.pv-current { margin-top: 12px; padding: 9px 10px; border-radius: var(--radius-sm); background: var(--surface-muted); color: var(--text-secondary); }.pv-current strong { margin: 0 5px; font-family: var(--font-mono); color: var(--text-primary); }.pv-command { display: flex; gap: 8px; margin-top: 12px; }.pv-command input { flex: 1; min-width: 0; }.pv-node-actions { display: flex; justify-content: flex-end; margin-top: 8px; }.pv-readonly { display: block; margin-top: 8px; color: var(--danger-text); }.pv-empty { margin-top: 14px; padding: 42px; text-align: center; }.pv-empty h3 { margin: 0 0 8px; }.pv-empty p { margin: 0; color: var(--text-muted); }
.pv-audit { margin-top: 18px; overflow: auto; }.pv-audit-header { display: flex; align-items: center; justify-content: space-between; gap: 16px; padding: 14px 16px; border-bottom: 1px solid var(--border-light); }.pv-audit h3 { margin: 0; font-size: var(--fs-lg); }.pv-audit-header p { margin: 4px 0 0; color: var(--text-muted); font-size: var(--fs-sm); }.pv-audit-actions { display: flex; flex: 0 0 auto; gap: 8px; }.pv-audit table { width: 100%; border-collapse: collapse; min-width: 1060px; }.pv-audit th, .pv-audit td { padding: 10px 14px; border-bottom: 1px solid var(--border-light); text-align: left; font-size: var(--fs-sm); }.pv-audit th { background: var(--surface-muted); color: var(--text-muted); }.pv-audit-detail { min-width: 230px; max-width: 420px; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }.command-ok { color: var(--success-text); }.command-error { color: var(--danger-text); }.pv-no-logs { color: var(--text-muted); text-align: center !important; }
.readback-match { color: var(--success-text); }
.readback-error { color: var(--danger-text); }
.readback-neutral { color: var(--text-muted); }
.pv-node-action { inline-size: 100px; min-inline-size: 100px; min-height: 36px; padding: 0; }
@media (max-width: 720px) { .parameter-view { padding: 18px 14px; }.pv-header, .pv-filter, .pv-audit-header { align-items: stretch; flex-direction: column; }.pv-filter select { min-width: 0; }.pv-audit-actions { justify-content: flex-end; }.pv-grid { grid-template-columns: 1fr; } }
</style>
