<template>
  <div class="parameter-view">
    <header class="pv-header">
      <div>
        <h2>参数在线下发</h2>
        <p>由已启用通信资源采集到的变量自动生成；不需要重复填写通信地址。</p>
      </div>
      <button class="btn btn-primary" :disabled="loading" @click="loadAll">{{ loading ? '同步中…' : '同步通信资源' }}</button>
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
          <button class="btn btn-primary" :disabled="submittingId === parameter.id || Boolean(commandBlockReason(parameter))" @click="requestWrite(parameter)">
            {{ submittingId === parameter.id ? '下发中…' : '确认下发' }}
          </button>
        </div>
        <small v-if="commandBlockReason(parameter)" class="pv-readonly">{{ commandBlockReason(parameter) }}</small>
      </article>
    </section>
    <section v-else class="pv-empty">
      <h3>暂无可下发资源</h3>
      <p>请先在“通信资源”中启用 OPC UA、Modbus TCP、UDP 或软件定义通信采集。采集到变量后，本页会自动生成对应的下发资源。</p>
    </section>

    <section class="pv-audit">
      <h3>本次会话下发记录</h3>
      <table>
        <thead><tr><th>时间</th><th>变量</th><th>协议</th><th>值</th><th>回读确认</th><th>结果</th></tr></thead>
        <tbody>
          <tr v-for="item in commands" :key="item.id">
            <td>{{ item.time }}</td><td>{{ item.name }}</td><td>{{ protocolLabel(item.source) }}</td><td>{{ item.value }}</td>
            <td :class="readbackClass(item)">{{ readbackLabel(item) }}</td>
            <td :class="item.ok ? 'command-ok' : 'command-error'">{{ item.message }}</td>
          </tr>
          <tr v-if="!commands.length"><td colspan="6" class="pv-no-logs">暂无下发记录</td></tr>
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
  </div>
</template>

<script setup>
import { computed, onMounted, reactive, ref } from 'vue'
import api from '../api/index.js'
import { useProjectStore } from '../store/projectStore.js'
import ConfirmDialog from '../components/common/ConfirmDialog.vue'
import { writeCollectedVariable } from '../services/automaticParameterService.js'

const projectStore = useProjectStore()
const resources = ref([])
const variables = ref([])
const loading = ref(false)
const submittingId = ref(null)
const selectedScope = ref('all')
const pendingCommand = ref(null)
const commands = ref([])
const draftValues = reactive({})
const message = reactive({ ok: true, text: '' })
const collectedSources = new Set(['OPCUA', 'MODBUS', 'UDP', 'SDC'])

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
  return String(variable.source).toUpperCase() === 'MODBUS' && (binding.includes('input_register') || binding.includes('discrete_input'))
}

function commandBlockReason(variable) {
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
    const [resourceResult, variableResult, realtimeResult] = await Promise.all([api.getCommunicationResources(), api.getVariables(), api.getRealtime()])
    resources.value = Array.isArray(resourceResult.data) ? resourceResult.data : []
    const realtimeById = new Map((Array.isArray(realtimeResult.data) ? realtimeResult.data : []).map((point) => [String(point.varId), point]))
    variables.value = (Array.isArray(variableResult.data) ? variableResult.data : []).map((variable) => {
      const latest = realtimeById.get(String(variable.id))
      return latest ? { ...variable, value: latest.value, quality: latest.quality, timestamp: latest.timestamp } : variable
    })
    variables.value.forEach((variable) => {
      if (!(variable.id in draftValues)) draftValues[variable.id] = Number(variable.value) || 0
    })
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
    message.ok = true
    message.text = `“${command.parameter.name}”已通过 ${protocolLabel(command.parameter.source)} 下发；${readbackLabel({ readback })}。`
    commands.value.unshift({ id: `${Date.now()}-${command.parameter.id}`, time: new Date().toLocaleString('zh-CN', { hour12: false }), name: command.parameter.name, source: command.parameter.source, value: command.value, ok: true, message: '下发成功', readback })
  } catch (error) {
    message.ok = false
    message.text = `下发失败：${error?.message || '控制器未确认写入'}`
    commands.value.unshift({ id: `${Date.now()}-${command.parameter.id}`, time: new Date().toLocaleString('zh-CN', { hour12: false }), name: command.parameter.name, source: command.parameter.source, value: command.value, ok: false, message: error?.message || '下发失败', readback: { status: 'NOT_EXECUTED', hasValue: false, message: '下发未成功，未执行回读' } })
  } finally {
    submittingId.value = null
  }
}

onMounted(loadAll)
</script>

<style scoped>
.parameter-view { height: calc(100vh - var(--header-height) - var(--statusbar-height)); overflow: auto; padding: 24px 28px 42px; background: var(--bg-secondary); color: var(--text-primary); }
.pv-header, .pv-notice, .pv-filter, .pv-grid, .pv-empty, .pv-audit, .pv-message { max-width: 1320px; margin-left: auto; margin-right: auto; }.pv-header { display: flex; justify-content: space-between; align-items: flex-start; gap: 16px; margin-bottom: 16px; }.pv-header h2 { margin: 0 0 6px; font-size: var(--fs-2xl); }.pv-header p { margin: 0; color: var(--text-muted); }.pv-notice, .pv-filter, .pv-audit, .pv-empty { border: 1px solid var(--border-light); border-radius: var(--radius-md); background: var(--bg-card); }.pv-notice { padding: 12px 14px; line-height: 1.6; color: var(--text-secondary); }.pv-filter { display: flex; align-items: center; gap: 10px; margin-top: 14px; padding: 12px 14px; }.pv-filter span { color: var(--text-muted); font-size: var(--fs-sm); }.pv-filter select { min-width: 230px; }.pv-message { margin-bottom: 12px; padding: 10px 12px; border: 1px solid var(--success-border); border-radius: var(--radius-md); background: var(--success-bg); color: var(--success-text); }.pv-message.error { border-color: var(--danger-border); background: var(--danger-bg); color: var(--danger-text); }.pv-grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(300px, 1fr)); gap: 14px; margin-top: 14px; }.pv-card { padding: 15px; border: 1px solid var(--border-light); border-radius: var(--radius-md); background: var(--bg-card); box-shadow: var(--shadow-xs); }.pv-card-title { display: flex; gap: 8px; align-items: center; justify-content: space-between; margin-bottom: 12px; }.pv-protocol { padding: 3px 8px; border-radius: 999px; background: var(--surface-muted); color: var(--text-secondary); font-size: 11px; }.pv-protocol.opcua { background: #e7f0ff; color: #245bab; }.pv-protocol.modbus { background: #f7edda; color: #8d5b12; }.pv-protocol.udp { background: #efe7ff; color: #6a43a3; }.pv-protocol.sdc { background: #dff4ec; color: #1e7855; }.pv-meta { display: grid; grid-template-columns: 76px minmax(0, 1fr); gap: 8px; margin: 7px 0; font-size: var(--fs-sm); }.pv-meta > span { color: var(--text-muted); }.pv-meta code { overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }.pv-meta b.good { color: var(--success-text); }.pv-meta b.bad { color: var(--danger-text); }.pv-current { margin-top: 12px; padding: 9px 10px; border-radius: var(--radius-sm); background: var(--surface-muted); color: var(--text-secondary); }.pv-current strong { margin: 0 5px; font-family: var(--font-mono); color: var(--text-primary); }.pv-command { display: flex; gap: 8px; margin-top: 12px; }.pv-command input { flex: 1; min-width: 0; }.pv-readonly { display: block; margin-top: 8px; color: var(--danger-text); }.pv-empty { margin-top: 14px; padding: 42px; text-align: center; }.pv-empty h3 { margin: 0 0 8px; }.pv-empty p { margin: 0; color: var(--text-muted); }.pv-audit { margin-top: 18px; overflow: auto; }.pv-audit h3 { margin: 0; padding: 14px 16px; border-bottom: 1px solid var(--border-light); font-size: var(--fs-lg); }.pv-audit table { width: 100%; border-collapse: collapse; min-width: 680px; }.pv-audit th, .pv-audit td { padding: 10px 14px; border-bottom: 1px solid var(--border-light); text-align: left; font-size: var(--fs-sm); }.pv-audit th { background: var(--surface-muted); color: var(--text-muted); }.command-ok { color: var(--success-text); }.command-error { color: var(--danger-text); }.pv-no-logs { color: var(--text-muted); text-align: center !important; }
.readback-match { color: var(--success-text); }
.readback-error { color: var(--danger-text); }
.readback-neutral { color: var(--text-muted); }
@media (max-width: 720px) { .parameter-view { padding: 18px 14px; }.pv-header, .pv-filter { align-items: stretch; flex-direction: column; }.pv-filter select { min-width: 0; }.pv-grid { grid-template-columns: 1fr; } }
</style>
