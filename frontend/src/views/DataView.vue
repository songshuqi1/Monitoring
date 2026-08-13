<template>
  <div class="data-view">
    <div class="dv-header">
      <h2>数据视图</h2>
      <div class="dv-actions">
        <span class="dv-count">变量: {{ store.variables.length }} | 显示: {{ filteredVariables.length }} | 在线: {{ variableStats.good }}</span>
        <button class="btn btn-sm" @click="refreshAll">⟳ 刷新</button>
      </div>
    </div>

    <div class="dv-tabs">
      <button v-for="tab in tabs" :key="tab.key"
        :class="['dv-tab', { active: activeTab === tab.key }]" @click="activeTab = tab.key">
        {{ tab.label }}
      </button>
    </div>

    <!-- 变量管理 -->
    <div v-show="activeTab === 'realtime'" class="dv-section variable-manager-section">
      <div class="variable-manager-toolbar">
        <div class="variable-search-box">
          <label>搜索变量</label>
          <input v-model.trim="variableSearch" type="search" placeholder="变量名、描述、来源、单位" />
        </div>
        <div class="variable-filter-box">
          <label>分组方式</label>
          <select v-model="variableGroupBy">
            <option value="custom">自定义分组</option>
            <option value="source">按来源</option>
            <option value="quality">按质量</option>
            <option value="unit">按单位</option>
            <option value="prefix">按名称前缀</option>
            <option value="none">不分组</option>
          </select>
        </div>
        <div class="variable-filter-box">
          <label>质量筛选</label>
          <select v-model="variableQualityFilter">
            <option value="all">全部质量</option>
            <option value="good">正常</option>
            <option value="uncertain">不确定</option>
            <option value="bad">故障</option>
            <option value="offline">离线</option>
          </select>
        </div>
        <div class="variable-manager-actions">
          <button type="button" class="btn btn-sm" @click="expandAllVariableGroups">全部展开</button>
          <button type="button" class="btn btn-sm" @click="collapseAllVariableGroups">全部折叠</button>
        </div>
      </div>

      <div class="variable-stat-grid">
        <div class="variable-stat-card"><strong>{{ variableStats.total }}</strong><span>总变量</span></div>
        <div class="variable-stat-card good"><strong>{{ variableStats.good }}</strong><span>正常</span></div>
        <div class="variable-stat-card uncertain"><strong>{{ variableStats.uncertain }}</strong><span>不确定</span></div>
        <div class="variable-stat-card bad"><strong>{{ variableStats.bad + variableStats.offline }}</strong><span>故障/离线</span></div>
        <div class="variable-stat-card"><strong>{{ groupedVariables.length }}</strong><span>分组</span></div>
      </div>

      <div v-if="groupedVariables.length === 0" class="td-empty variable-empty">
        {{ store.variables.length ? '没有匹配的变量' : '暂无变量，请先在「通信资源」配置数据源并启动采集' }}
      </div>

      <div v-else class="variable-group-list">
        <section v-for="group in groupedVariables" :key="group.key" class="variable-group-card">
          <button type="button" class="variable-group-header" @click="toggleVariableGroup(group.key)">
            <span class="group-toggle">{{ isVariableGroupCollapsed(group.key) ? '▸' : '▾' }}</span>
            <span class="group-title">{{ group.label }}</span>
            <span class="group-summary">
              {{ group.items.length }} 个变量 · 正常 {{ group.good }} · 异常 {{ group.bad + group.offline }}
            </span>
          </button>

          <table v-show="!isVariableGroupCollapsed(group.key)" class="dv-table variable-table">
            <thead>
              <tr>
                <th>变量名</th>
                <th>自定义分组</th>
                <th>描述</th>
                <th>值</th>
                <th>单位</th>
                <th>质量</th>
                <th>来源</th>
                <th>OPC UA 节点路径</th>
                <th>操作</th>
              </tr>
            </thead>
            <tbody>
              <tr v-for="v in group.items" :key="v.id">
                <td>
                  <span class="var-name-dot" :style="{ background: v.color || '#4f6fb8' }"></span>
                  <span class="var-name-text" :title="v.name">{{ v.name }}</span>
                </td>
                <td class="td-group-editor">
                  <input
                    :value="variableGroupMap[v.id] || ''"
                    placeholder="未分组"
                    @change="setVariableGroup(v.id, $event.target.value)"
                  />
                </td>
                <td class="td-muted">{{ v.description || '-' }}</td>
                <td class="td-value" :style="{ color: getReadableColor(v.color) }">{{ getDisplayValue(v.id) }}</td>
                <td class="td-muted">{{ v.unit || '-' }}</td>
                <td>
                  <span class="quality-badge" :class="getQuality(v.id)">
                    <span class="q-dot"></span>{{ getQualityText(v.id) }}
                  </span>
                </td>
                <td><span class="source-tag">{{ v.source || '-' }}</span></td>
                <td class="td-mono">\Root\Objects\{{ v.name }}</td>
                <td class="td-actions">
                  <button class="btn-action" @click="openWriteDialog(v)">写入</button>
                  <button class="btn-action btn-action-del" @click="deleteVariable(v)">删除</button>
                </td>
              </tr>
            </tbody>
          </table>
        </section>
      </div>
    </div>

    <!-- 报警 -->
    <div v-show="activeTab === 'alarms'" class="dv-section">
      <table class="dv-table">
        <thead><tr><th>时间</th><th>变量</th><th>类型</th><th>触发值</th><th>限值</th><th>级别</th><th>信息</th><th>状态</th></tr></thead>
        <tbody>
          <tr v-for="alarm in store.alarms" :key="alarm.id">
            <td class="td-mono">{{ formatTime(alarm.occurredAtMs || alarm.occurredAt) }}</td>
            <td>{{ alarm.varName }}</td>
            <td><span class="badge" :class="['HH','LL'].includes(alarm.alarmType) ? 'badge-err' : 'badge-warn'">{{ alarm.alarmType }}</span></td>
            <td class="td-value">{{ formatValue(alarm.alarmValue) }}</td>
            <td class="td-mono">{{ formatValue(alarm.limitValue) }}</td>
            <td>{{ alarm.severity }}</td>
            <td class="td-muted">{{ alarm.message }}</td>
            <td><span v-if="!alarm.acknowledged" class="badge badge-warn">未确认</span><span v-else class="badge badge-ok">已确认</span></td>
          </tr>
          <tr v-if="store.alarms.length === 0"><td colspan="8" class="td-empty">暂无报警</td></tr>
        </tbody>
      </table>
    </div>

    <!-- 操作日志 -->
    <div v-show="activeTab === 'logs'" class="dv-section">
      <table class="dv-table">
        <thead><tr><th>时间</th><th>类型</th><th>变量</th><th>旧值</th><th>新值</th><th>操作员</th><th>详情</th></tr></thead>
        <tbody>
          <tr v-for="log in logs" :key="log.id">
            <td class="td-mono">{{ formatTime(log.operatedAtMs || log.operatedAt) }}</td>
            <td><span class="badge badge-warn">{{ log.operationType }}</span></td>
            <td>{{ log.varName }}</td>
            <td class="td-value">{{ formatValue(log.oldValue) }}</td>
            <td class="td-value" style="color:var(--accent)">{{ formatValue(log.newValue) }}</td>
            <td class="td-muted">{{ log.operator_ || log.operator || '-' }}</td>
            <td class="td-muted">{{ log.detail }}</td>
          </tr>
          <tr v-if="logs.length === 0"><td colspan="7" class="td-empty">暂无操作日志</td></tr>
        </tbody>
      </table>
    </div>

    <!-- 数据库历史 -->
    <div v-show="activeTab === 'history'" class="dv-section">
      <div class="history-toolbar">
        <span class="history-summary">
          宽表历史：{{ Math.max(historyColumns.length - 2, 0) }} 个变量 / {{ historyRows.length }} 行
        </span>
        <button class="btn btn-sm btn-primary" :disabled="historyLoading" @click="loadHistoryDb">
          {{ historyLoading ? '查询中...' : '刷新宽表' }}
        </button>
      </div>
      <table class="dv-table history-wide-table">
        <thead>
          <tr>
            <th v-for="col in historyColumns" :key="col">{{ col }}</th>
          </tr>
        </thead>
        <tbody>
          <tr v-for="(row, rowIndex) in historyRows" :key="`${row.id}-${row.time}-${rowIndex}`">
            <td v-for="col in historyColumns" :key="col" :class="getHistoryCellClass(col)">
              {{ formatHistoryCell(row, col) }}
            </td>
          </tr>
          <tr v-if="historyRows.length === 0">
            <td :colspan="historyColumns.length || 2" class="td-empty">{{ historyLoading ? '查询中...' : '暂无数据库历史数据' }}</td>
          </tr>
        </tbody>
      </table>
    </div>

    <!-- 写入对话框 -->
    <div v-if="showWriteDialog" class="dialog-overlay" @click.self="showWriteDialog = false">
      <div class="dialog">
        <h3>写入变量 - {{ writeVar?.name }}</h3>
        <div class="form-group">
          <label>当前值: <strong>{{ getDisplayValue(writeVar?.id) }}</strong></label>
        </div>
        <div class="form-group">
          <label>新值</label>
          <input type="number" v-model.number="writeValue" step="0.1" />
        </div>
        <div class="dialog-actions">
          <button class="btn btn-secondary" @click="showWriteDialog = false">取消</button>
          <button class="btn btn-primary" @click="doWrite">写入</button>
        </div>
      </div>
    </div>

    <ConfirmDialog
      v-if="deleteTarget"
      title="删除变量"
      :message="`确定删除变量「${deleteTarget.name}」？`"
      detail="删除后将同时清除该变量的历史数据和报警记录。"
      confirm-text="确认删除"
      submitting-text="删除中..."
      danger
      @close="deleteTarget = null"
      @confirm="confirmDeleteVariable"
    />
  </div>
</template>

<script>
import { ref, computed, onMounted, onUnmounted, watch } from 'vue'
import { useMonitorStore } from '../store/index.js'
import api from '../api/index.js'
import ConfirmDialog from '../components/common/ConfirmDialog.vue'

const VARIABLE_GROUP_STORAGE_KEY = 'monitor-variable-groups-v1'

function loadVariableGroupMap() {
  try {
    const raw = localStorage.getItem(VARIABLE_GROUP_STORAGE_KEY)
    const parsed = raw ? JSON.parse(raw) : {}
    return parsed && typeof parsed === 'object' ? parsed : {}
  } catch {
    return {}
  }
}

function saveVariableGroupMap(map) {
  try {
    localStorage.setItem(VARIABLE_GROUP_STORAGE_KEY, JSON.stringify(map || {}))
  } catch {}
}

export default {
  name: 'DataView',
  components: { ConfirmDialog },
  setup() {
    const store = useMonitorStore()
    const logs = ref([])
    const activeTab = ref('realtime')
    const showWriteDialog = ref(false)
    const writeVar = ref(null)
    const writeValue = ref(0)
    const historyColumns = ref(['id', 'time'])
    const historyRows = ref([])
    const historyLoading = ref(false)
    const deleteTarget = ref(null)
    const variableSearch = ref('')
    const variableGroupBy = ref(localStorage.getItem('monitor-variable-group-by') || 'custom')
    const variableQualityFilter = ref('all')
    const collapsedVariableGroups = ref({})
    const variableGroupMap = ref(loadVariableGroupMap())

    const tabs = [
      { key: 'realtime', label: '变量管理' },
      { key: 'alarms', label: '报警记录' },
      { key: 'logs', label: '操作日志' },
      { key: 'history', label: '数据库历史' },
    ]

    function getReadableColor(color) {
      if (!color) return '#4f6fb8'
      const c = color.replace('#', '')
      const r = parseInt(c.substring(0,2), 16), g = parseInt(c.substring(2,4), 16), b = parseInt(c.substring(4,6), 16)
      return (r * 299 + g * 587 + b * 114) / 1000 > 180 ? '#4f6fb8' : color
    }

    const getDisplayValue = (id) => {
      const dp = store.realtimeData[id]
      return dp && dp.value !== undefined ? formatValue(dp.value) : '---'
    }
    const getQualityKey = (id) => {
      const dp = store.realtimeData[id]
      if (!dp) return 'offline'
      const q = String(dp.quality || '').toUpperCase()
      if (q === 'GOOD') return 'good'
      if (q === 'UNCERTAIN') return 'uncertain'
      return 'bad'
    }
    const getQuality = (id) => {
      const key = getQualityKey(id)
      return key === 'offline' ? 'bad' : key
    }
    const getQualityText = (id) => {
      const key = getQualityKey(id)
      if (key === 'good') return '正常'
      if (key === 'uncertain') return '不确定'
      if (key === 'offline') return '离线'
      return '故障'
    }
    const formatTime = (ms) => {
      if (!ms) return ''
      return new Date(ms).toLocaleString('zh-CN', { hour12: false })
    }
    const formatValue = (value) => {
      if (value === null || value === undefined || value === '') return '-'
      const num = Number(value)
      return Number.isFinite(num) ? num.toFixed(2) : String(value)
    }
    const getHistoryQuality = (quality) => {
      const q = (quality || 'GOOD').toUpperCase()
      return q === 'GOOD' ? 'good' : (q === 'UNCERTAIN' ? 'uncertain' : 'bad')
    }
    const getHistoryCellClass = (col) => {
      if (col === 'id' || col === 'time') return 'td-mono'
      return 'td-value'
    }
    const formatHistoryCell = (row, col) => {
      if (col === 'time') return formatTime(row.time)
      if (col === 'id') return row.id ?? ''
      const value = row[col]
      if (value === null || value === undefined || value === '') return ''
      return formatValue(value)
    }

    const variableStats = computed(() => {
      const stats = { total: store.variables.length, good: 0, uncertain: 0, bad: 0, offline: 0 }
      store.variables.forEach(v => {
        const key = getQualityKey(v.id)
        stats[key] += 1
      })
      return stats
    })

    function variablePrefix(name) {
      const text = String(name || '').trim()
      if (!text) return '未命名'
      const parts = text.split(/[._\-/:\\]/).filter(Boolean)
      if (parts.length > 1) return parts[0]
      const matched = text.match(/^[A-Za-z]+|^[\u4e00-\u9fa5]{1,3}/)
      return matched?.[0] || text.slice(0, 3)
    }

    function variableGroupLabel(v) {
      if (variableGroupBy.value === 'none') return '全部变量'
      if (variableGroupBy.value === 'source') return v.source || '未知来源'
      if (variableGroupBy.value === 'quality') return getQualityText(v.id)
      if (variableGroupBy.value === 'unit') return v.unit || '无单位'
      if (variableGroupBy.value === 'prefix') return variablePrefix(v.name)
      return variableGroupMap.value[v.id] || '未分组'
    }

    const filteredVariables = computed(() => {
      const q = variableSearch.value.trim().toLowerCase()
      return store.variables.filter(v => {
        const quality = getQualityKey(v.id)
        if (variableQualityFilter.value !== 'all' && quality !== variableQualityFilter.value) return false
        if (!q) return true
        return [v.name, v.description, v.source, v.unit, v.id, getQualityText(v.id), variableGroupMap.value[v.id]]
          .some(value => String(value || '').toLowerCase().includes(q))
      })
    })

    const groupedVariables = computed(() => {
      const groups = new Map()
      filteredVariables.value.forEach(v => {
        const label = variableGroupLabel(v)
        const key = `${variableGroupBy.value}:${label}`
        if (!groups.has(key)) groups.set(key, { key, label, items: [], good: 0, uncertain: 0, bad: 0, offline: 0 })
        const group = groups.get(key)
        group.items.push(v)
        group[getQualityKey(v.id)] += 1
      })
      return [...groups.values()]
        .map(group => ({ ...group, items: [...group.items].sort((a, b) => String(a.name || '').localeCompare(String(b.name || ''), 'zh-CN')) }))
        .sort((a, b) => {
          if (a.label === '未分组') return 1
          if (b.label === '未分组') return -1
          return String(a.label).localeCompare(String(b.label), 'zh-CN')
        })
    })

    function setVariableGroup(id, value) {
      const text = String(value || '').trim()
      const next = { ...variableGroupMap.value }
      if (text) next[id] = text
      else delete next[id]
      variableGroupMap.value = next
      saveVariableGroupMap(next)
    }

    function isVariableGroupCollapsed(key) {
      return Boolean(collapsedVariableGroups.value[key])
    }
    function toggleVariableGroup(key) {
      collapsedVariableGroups.value = { ...collapsedVariableGroups.value, [key]: !collapsedVariableGroups.value[key] }
    }
    function expandAllVariableGroups() {
      collapsedVariableGroups.value = {}
    }
    function collapseAllVariableGroups() {
      collapsedVariableGroups.value = Object.fromEntries(groupedVariables.value.map(group => [group.key, true]))
    }

    watch(variableGroupBy, (value) => {
      localStorage.setItem('monitor-variable-group-by', value)
      collapsedVariableGroups.value = {}
    })

    const openWriteDialog = (v) => {
      writeVar.value = v
      writeValue.value = store.realtimeData[v.id]?.value || 0
      showWriteDialog.value = true
    }

    const doWrite = async () => {
      if (!writeVar.value) return
      try { await api.writeVariable(writeVar.value.id, writeValue.value); showWriteDialog.value = false }
      catch (e) { console.error('Write failed:', e) }
    }

    const deleteVariable = (v) => {
      deleteTarget.value = v
    }

    const confirmDeleteVariable = async () => {
      if (!deleteTarget.value) return
      try {
        await api.deleteVariable(deleteTarget.value.id)
        const next = { ...variableGroupMap.value }
        delete next[deleteTarget.value.id]
        variableGroupMap.value = next
        saveVariableGroupMap(next)
        deleteTarget.value = null
        await store.loadVariables()
        await store.loadRealtime()
      }
      catch (e) { console.error('Delete failed:', e) }
    }

    const loadHistoryDb = async () => {
      historyLoading.value = true
      try {
        const res = await api.getHistoryDbWide(0, 0, 5000)
        historyColumns.value = res.data?.columns?.length ? res.data.columns : ['id', 'time']
        historyRows.value = Array.isArray(res.data?.rows) ? res.data.rows : []
      }
      catch (e) {
        console.error('Load history DB failed:', e)
        historyColumns.value = ['id', 'time']
        historyRows.value = []
      }
      finally { historyLoading.value = false }
    }

    const refreshAll = async () => {
      await store.loadRealtime(); await store.loadAlarms(); await loadLogs(); await store.loadVariables()
      if (activeTab.value === 'history') await loadHistoryDb()
    }

    const loadLogs = async () => { try { logs.value = (await api.getLogs()).data } catch {} }

    watch(activeTab, async (tab) => {
      if (tab === 'history') {
        if (historyRows.value.length === 0) await loadHistoryDb()
      }
    })

    let timer = null
    onMounted(async () => {
      await store.loadVariables(); await store.loadRealtime(); await store.loadAlarms(); await loadLogs()
      timer = setInterval(async () => { await store.loadRealtime(); await store.loadAlarms() }, 5000)
    })
    onUnmounted(() => { clearInterval(timer) })

    return {
      store, logs, activeTab, tabs, showWriteDialog, writeVar, writeValue,
      historyColumns, historyRows, historyLoading, deleteTarget,
      variableSearch, variableGroupBy, variableQualityFilter, variableGroupMap,
      variableStats, filteredVariables, groupedVariables,
      getReadableColor, getDisplayValue, getQuality, getQualityText, formatTime,
      formatValue, getHistoryQuality, getHistoryCellClass, formatHistoryCell,
      setVariableGroup, isVariableGroupCollapsed, toggleVariableGroup,
      expandAllVariableGroups, collapseAllVariableGroups,
      openWriteDialog, doWrite, deleteVariable, confirmDeleteVariable, refreshAll, loadHistoryDb
    }
  }
}
</script>

<style scoped>
.data-view { padding: 24px 28px; height: calc(100vh - var(--header-height) - var(--statusbar-height)); overflow-y: auto; background: var(--bg-secondary); }
.dv-header { display: flex; justify-content: space-between; align-items: center; gap: 16px; max-width: var(--content-max-width); margin: 0 auto 18px; }
.dv-header h2 { font-size: var(--fs-2xl); font-weight: var(--fw-bold); color: var(--text-primary); line-height: var(--lh-tight); }
.dv-actions { display: flex; align-items: center; gap: 12px; }
.dv-count { display: inline-flex; align-items: center; min-height: 30px; padding: 0 12px; border-radius: var(--radius-full); border: 1px solid var(--border-light); background: var(--bg-primary); font-size: var(--fs-sm); color: var(--text-tertiary); font-family: var(--font-mono); }
.dv-tabs { display: flex; gap: 6px; max-width: var(--content-max-width); margin: 0 auto 14px; padding: 4px; border: 1px solid var(--border-light); border-radius: var(--radius-md); background: var(--bg-primary); box-shadow: var(--shadow-xs); }
.dv-tab { min-height: 38px; padding: 0 18px; font-size: var(--fs-md); font-weight: var(--fw-semibold); color: var(--text-tertiary); border: 1px solid transparent; border-radius: var(--radius-sm); transition: background var(--transition-fast), color var(--transition-fast), border-color var(--transition-fast); }
.dv-tab:hover { color: var(--text-primary); background: var(--bg-hover); }
.dv-tab.active { color: var(--accent); border-color: var(--info-border); background: var(--accent-soft); }
.dv-section { max-width: var(--content-max-width); margin: 0 auto; animation: fadeIn 0.2s ease; background: var(--bg-primary); border: 1px solid var(--border-light); border-radius: var(--radius-lg); box-shadow: var(--shadow-card); overflow: auto; }
.variable-manager-section { padding: 14px; }
.variable-manager-toolbar {
  display: grid;
  grid-template-columns: minmax(240px, 1.5fr) 150px 150px auto;
  gap: 12px;
  align-items: end;
  padding: 12px;
  border: 1px solid var(--border-light);
  border-radius: var(--radius-md);
  background: var(--surface-muted);
}
.variable-search-box,
.variable-filter-box {
  min-width: 0;
  display: flex;
  flex-direction: column;
  gap: 6px;
}
.variable-search-box label,
.variable-filter-box label {
  color: var(--text-secondary);
  font-size: var(--fs-sm);
  font-weight: var(--fw-bold);
}
.variable-search-box input,
.variable-filter-box select,
.td-group-editor input {
  min-height: 34px;
  font-size: var(--fs-sm);
}
.variable-manager-actions {
  display: flex;
  align-items: center;
  gap: 8px;
  justify-content: flex-end;
}
.variable-stat-grid {
  display: grid;
  grid-template-columns: repeat(5, minmax(0, 1fr));
  gap: 10px;
  margin: 12px 0;
}
.variable-stat-card {
  min-height: 58px;
  display: flex;
  flex-direction: column;
  justify-content: center;
  gap: 2px;
  padding: 9px 12px;
  border: 1px solid var(--border-light);
  border-radius: var(--radius-md);
  background: #ffffff;
}
.variable-stat-card strong { color: var(--text-primary); font-family: var(--font-mono); font-size: var(--fs-xl); line-height: 1; }
.variable-stat-card span { color: var(--text-tertiary); font-size: var(--fs-sm); font-weight: var(--fw-semibold); }
.variable-stat-card.good strong { color: var(--success-text); }
.variable-stat-card.uncertain strong { color: #a66a1f; }
.variable-stat-card.bad strong { color: var(--danger-text); }
.variable-group-list { display: flex; flex-direction: column; gap: 12px; }
.variable-group-card {
  overflow: hidden;
  border: 1px solid var(--border-light);
  border-radius: var(--radius-md);
  background: #ffffff;
}
.variable-group-header {
  width: 100%;
  min-height: 42px;
  display: flex;
  align-items: center;
  gap: 9px;
  padding: 0 13px;
  border-bottom: 1px solid var(--border-light);
  background: var(--gradient-pane);
  color: var(--text-secondary);
  text-align: left;
}
.group-toggle { width: 16px; color: var(--text-placeholder); font-size: var(--fs-sm); }
.group-title { color: var(--text-primary); font-size: var(--fs-md); font-weight: var(--fw-bold); }
.group-summary { margin-left: auto; color: var(--text-tertiary); font-size: var(--fs-sm); }
.variable-table { min-width: 1120px; border-radius: 0; }
.dv-table { width: 100%; border-collapse: separate; border-spacing: 0; font-size: var(--fs-md); }
.dv-table thead th { padding: 11px 12px; text-align: left; font-weight: var(--fw-semibold); color: var(--text-secondary); font-size: var(--fs-sm); letter-spacing: 0; border-bottom: 1px solid var(--border-light); background: var(--surface-muted); position: sticky; top: 0; }
.dv-table tbody td { padding: 10px 12px; border-bottom: 1px solid var(--border-light); vertical-align: middle; }
.dv-table tbody tr:nth-child(even) { background: rgba(247,248,251,0.52); }
.dv-table tbody tr:hover { background: var(--bg-hover); }
.variable-table tbody tr:last-child td { border-bottom: 0; }
.td-muted { color: var(--text-placeholder); }
.td-value { font-family: var(--font-mono); font-weight: var(--fw-medium); }
.td-mono { font-family: var(--font-mono); font-size: var(--fs-sm); color: var(--text-tertiary); }
.td-empty { text-align: center; padding: 40px 0; color: var(--text-placeholder); font-size: var(--fs-sm); }
.variable-empty { border: 1px dashed var(--border-color); border-radius: var(--radius-md); background: var(--surface-muted); }
.var-name-dot { display: inline-block; width: 8px; height: 8px; border-radius: 50%; margin-right: 6px; vertical-align: middle; }
.var-name-text { display: inline-block; max-width: 180px; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; vertical-align: middle; font-weight: var(--fw-semibold); }
.q-dot { width: 5px; height: 5px; border-radius: 50%; background: currentColor; }
.source-tag { background: var(--surface-muted); border-color: var(--border-light); color: var(--text-tertiary); }
.td-group-editor { min-width: 132px; }
.td-group-editor input { height: 32px; padding: 0 9px; border-radius: var(--radius-sm); background: #ffffff; }
.td-actions { display: flex; gap: 5px; white-space: nowrap; }
.btn-action { min-height: 31px; padding: 0 12px; font-size: var(--fs-sm); font-weight: var(--fw-semibold); border: 1px solid var(--info-border); color: var(--accent); border-radius: var(--radius-sm); background: var(--accent-soft); cursor: pointer; transition: background var(--transition-fast), border-color var(--transition-fast), color var(--transition-fast); }
.btn-action:hover { background: var(--accent); border-color: var(--accent); color: var(--text-inverse); }
.btn-action-del { border-color: var(--danger-border); color: var(--danger-text); background: var(--danger-bg); }
.btn-action-del:hover { background: var(--color-offline); border-color: var(--color-offline); color: var(--text-inverse); }
.history-toolbar { display: flex; align-items: center; justify-content: space-between; gap: 12px; margin: 0 0 12px; padding: 14px 14px 0; min-width: 720px; }
.history-summary { display: inline-flex; align-items: center; min-height: 28px; padding: 0 10px; border-radius: var(--radius-full); border: 1px solid var(--border-light); background: var(--surface-muted); color: var(--text-tertiary); font-size: var(--fs-sm); font-family: var(--font-mono); }
.history-wide-table { min-width: 720px; }
.history-wide-table th,
.history-wide-table td { white-space: nowrap; }

@media (max-width: 980px) {
  .variable-manager-toolbar { grid-template-columns: 1fr 1fr; }
  .variable-manager-actions { justify-content: flex-start; }
  .variable-stat-grid { grid-template-columns: repeat(2, minmax(0, 1fr)); }
}
@media (max-width: 760px) {
  .data-view { padding: 16px 12px; }
  .dv-header { align-items: flex-start; flex-direction: column; }
  .dv-actions { width: 100%; justify-content: space-between; }
  .dv-tabs { overflow-x: auto; }
  .dv-tab { min-height: 40px; }
  .variable-manager-toolbar { grid-template-columns: 1fr; }
  .variable-stat-grid { grid-template-columns: 1fr; }
  .group-summary { display: none; }
  .history-toolbar { min-width: 0; flex-direction: column; align-items: stretch; }
  .history-summary { justify-content: center; }
  .history-wide-table { min-width: 640px; }
}

/* reuse global dialog/form styles */
</style>