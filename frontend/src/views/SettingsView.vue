<template>
  <div class="settings-view">
    <div class="sv-header">
      <h2>系统设置</h2>
      <span class="sv-hint">数据库连接和显示配置</span>
    </div>

    <div class="sv-grid">
      <!-- 数据库设置 -->
      <div class="sv-card db-card">
        <h3>🗄️ 数据库连接</h3>
        <div class="db-status-bar" :class="dbConnected ? 'connected' : ''">
          <span class="db-status-dot"></span>
          <span>{{ dbStatusText }}</span>
          <button v-if="dbConnected" class="btn btn-sm" style="margin-left:auto" @click="dbDisconnect">断开</button>
        </div>
        <div class="form-group">
          <label>主机地址</label>
          <input v-model.trim="dbForm.host" autocomplete="off" placeholder="请输入主机地址" />
        </div>
        <div class="form-row">
          <div class="form-group">
            <label>端口</label>
            <input type="number" v-model.number="dbForm.port" min="1" max="65535" placeholder="请输入端口" />
          </div>
          <div class="form-group">
            <label>数据库名</label>
            <input v-model.trim="dbForm.dbname" autocomplete="off" placeholder="请输入数据库名" />
          </div>
        </div>
        <div class="form-row">
          <div class="form-group">
            <label>用户名</label>
            <input v-model.trim="dbForm.user" autocomplete="off" placeholder="请输入用户名" />
          </div>
          <div class="form-group">
            <label>密码</label>
            <input type="password" v-model="dbForm.pass" autocomplete="new-password" placeholder="请输入密码（可为空）" />
          </div>
        </div>
        <div class="db-actions">
          <button class="btn" @click="testDbConnect" :disabled="dbTesting">
            {{ dbTesting ? '连接中...' : '🔍 测试并连接' }}
          </button>
          <button class="btn btn-primary" @click="saveDbConfig">💾 保存设置</button>
        </div>
        <div v-if="dbResult" class="db-test-result" :class="dbResult.ok ? 'ok' : 'err'">{{ dbResult.msg }}</div>
        <div v-if="saveMessage" class="db-test-result ok">{{ saveMessage }}</div>
      </div>

      <!-- 显示设置 -->
      <div class="sv-card">
        <h3>🎨 显示设置</h3>
        <div class="form-row">
          <div class="form-group">
            <label>图表刷新间隔 (ms)</label>
            <input type="number" v-model.number="config.chartRefresh" />
          </div>
          <div class="form-group">
            <label>历史数据保留 (天)</label>
            <input type="number" v-model.number="config.historyRetention" />
          </div>
        </div>
      </div>

      <!-- 服务状态 -->
      <div class="sv-card">
        <h3>📊 服务状态</h3>
        <div class="status-table">
          <div class="status-row" v-for="s in statusItems" :key="s.key">
            <span>{{ s.label }}</span>
            <span class="status-val" :class="s.value ? 'on' : 'off'">
              <span class="s-dot"></span> {{ s.text }}
            </span>
          </div>
        </div>
        <button class="btn btn-sm" style="margin-top:12px;width:100%;" @click="refreshStatus">⟳ 刷新状态</button>
      </div>

      <!-- 局域网访问 -->
      <div class="sv-card network-card">
        <h3>🌐 局域网访问</h3>
        <p class="network-hint">后端在本机运行时，同一局域网内的电脑使用 {{ networkUrls.primaryUrl || 'http://202.118.21.28:8081' }} 访问平台。</p>
        <div v-if="networkUrls.frontendUrls.length" class="network-url-list">
          <div v-for="url in networkUrls.frontendUrls" :key="url" class="network-url-row">
            <code>{{ url }}</code>
            <button class="btn btn-xs" @click="copyUrl(url)">复制</button>
          </div>
        </div>
        <div v-else class="network-empty">未获取到局域网 IP，请检查网络适配器。</div>
        <div class="network-note">
          推荐统一使用上面的 {{ networkUrls.backendPort || 8081 }} 地址。开发模式前端端口为 {{ networkUrls.devFrontendPort || 5170 }}，需要 Windows 防火墙放行对应端口。
        </div>
        <div v-if="copyMessage" class="db-test-result ok">{{ copyMessage }}</div>
        <button class="btn btn-sm" style="margin-top:12px;width:100%;" @click="loadNetworkUrls">刷新访问地址</button>
      </div>
    </div>
  </div>
</template>

<script>
import { ref, reactive, onMounted, computed } from 'vue'
import { useMonitorStore } from '../store/index.js'
import api from '../api/index.js'

const DEFAULT_DB_CONFIG = {
  host: '',
  port: null,
  user: '',
  pass: '',
  dbname: ''
}
const DB_CONFIG_STORAGE_KEY = 'monitor-db-config-v2'

function sanitizeStoredDbConfig() {
  try {
    const saved = localStorage.getItem(DB_CONFIG_STORAGE_KEY)
    if (!saved) return
    const config = JSON.parse(saved)
    if (!config || typeof config !== 'object') return
    const { pass: _password, ...safeConfig } = config
    localStorage.setItem(DB_CONFIG_STORAGE_KEY, JSON.stringify(safeConfig))
  } catch {
    localStorage.removeItem(DB_CONFIG_STORAGE_KEY)
  }
}

sanitizeStoredDbConfig()

export default {
  name: 'SettingsView',
  setup() {
    const store = useMonitorStore()
    const dbForm = reactive({ ...DEFAULT_DB_CONFIG })
    const dbTesting = ref(false)
    const dbResult = ref(null)
    const saveMessage = ref('')
    const dbConnected = ref(false)
    const dbStatusText = ref('未检测')
    const copyMessage = ref('')
    const networkUrls = computed(() => store.networkUrls)

    // 加载本地保存的数据库配置
    function loadDbConfig() {
      try {
        const saved = localStorage.getItem(DB_CONFIG_STORAGE_KEY)
        if (saved) {
          const c = JSON.parse(saved)
          const { pass: _discardedPassword, ...safeConfig } = c || {}
          Object.assign(dbForm, { ...DEFAULT_DB_CONFIG, ...safeConfig, pass: '' })
          localStorage.setItem(DB_CONFIG_STORAGE_KEY, JSON.stringify(safeConfig))
        }
      } catch {}
    }

    const testDbConnect = async () => {
      dbTesting.value = true; dbResult.value = null
      if (!dbForm.host || !Number(dbForm.port) || !dbForm.user || !dbForm.dbname) {
        dbResult.value = { ok: false, msg: '请填写主机地址、端口、用户名和数据库名' }
        dbTesting.value = false
        return
      }
      try {
        const r = await api.dbConnect(dbForm.host, dbForm.port, dbForm.user, dbForm.pass, dbForm.dbname)
        dbConnected.value = r.data.connected
        dbStatusText.value = r.data.connected ? '数据库已连接' : '数据库未连接'
        dbResult.value = { ok: r.data.connected, msg: r.data.connected ? '✅ 连接成功' : '❌ ' + r.data.message }
        if (r.data.connected) {
          await refreshStatus()
          store.loadVariables()
        }
      } catch (e) {
        dbResult.value = { ok: false, msg: '❌ 连接失败: ' + (e.message || '无法连接到后端') }
      } finally { dbTesting.value = false }
    }

    const dbDisconnect = () => { dbConnected.value = false; dbStatusText.value = '未连接' }
    const saveDbConfig = () => {
      const { pass: _password, ...safeConfig } = { ...dbForm }
      localStorage.setItem(DB_CONFIG_STORAGE_KEY, JSON.stringify(safeConfig))
      saveMessage.value = '连接参数已保存，密码不会保存在浏览器中'
      window.setTimeout(() => { saveMessage.value = '' }, 2400)
    }

    const config = ref({ chartRefresh: 500, historyRetention: 30 })
    const status = computed(() => store.systemStatus)

    const statusItems = computed(() => [
      { key: 'opcuaServer', label: 'OPC UA 服务器', value: status.value.opcuaServer, text: status.value.opcuaServer ? '运行中' : '已停止' },
      { key: 'opcuaClient', label: 'OPC UA 客户端', value: status.value.opcuaClient, text: status.value.opcuaClient ? '已连接' : '未连接' },
      { key: 'udpReceiver', label: 'UDP 接收器', value: status.value.udpReceiver, text: status.value.udpReceiver ? '接收中' : '已停止' },
      { key: 'database', label: '数据库', value: status.value.database, text: status.value.database ? '已连接' : '未连接' },
      { key: 'ws', label: 'WebSocket', value: store.wsConnected, text: store.wsConnected ? '已连接' : '未连接' },
    ])

    const refreshStatus = async () => {
      try {
        await store.fetchStatus()
        dbConnected.value = Boolean(store.systemStatus.database)
        dbStatusText.value = dbConnected.value ? '数据库已连接' : '数据库未连接'
      }
      catch {}
    }

    const loadNetworkUrls = () => store.loadNetworkUrls()

    const copyUrl = async (url) => {
      try {
        await navigator.clipboard.writeText(url)
        copyMessage.value = '访问地址已复制'
      } catch {
        copyMessage.value = '复制失败，请手动选择地址'
      }
      window.setTimeout(() => { copyMessage.value = '' }, 2400)
    }

    onMounted(async () => {
      loadDbConfig()
      await refreshStatus()
      if (!store.residentReady || !store.networkUrls.primaryUrl) await loadNetworkUrls()
    })

    return {
      store, dbForm, dbTesting, dbResult, saveMessage, dbConnected, dbStatusText,
      testDbConnect, dbDisconnect, saveDbConfig, config, status, statusItems, refreshStatus,
      networkUrls, copyMessage, loadNetworkUrls, copyUrl
    }
  }
}
</script>

<style scoped>
.settings-view { padding: 24px 28px; font-size: var(--fs-md); height: calc(100vh - var(--header-height) - var(--statusbar-height)); overflow-y: auto; background: var(--bg-secondary); }
.sv-header { max-width: var(--content-max-width); display: flex; align-items: baseline; gap: 12px; margin: 0 auto 20px; }
.sv-header h2 { font-size: var(--fs-2xl); line-height: var(--lh-tight); font-weight: var(--fw-bold); color: var(--text-primary); }
.sv-hint { font-size: var(--fs-sm); color: var(--text-muted); }
.sv-grid { max-width: var(--content-max-width); margin: 0 auto; display: grid; grid-template-columns: minmax(0, 1.1fr) minmax(0, 0.9fr); gap: 16px; }
.sv-card { background: var(--bg-card); border: 1px solid var(--border-light); border-radius: var(--radius-lg); padding: 22px; box-shadow: var(--shadow-card); }
.sv-card h3 { font-size: var(--fs-xl); font-weight: var(--fw-bold); color: var(--text-primary); margin-bottom: 16px; padding-bottom: 12px; border-bottom: 1px solid var(--border-light); }
.db-status-bar { display: flex; align-items: center; gap: 8px; min-height: 44px; padding: 0 14px; margin-bottom: 16px; border-radius: var(--radius-md); font-size: var(--fs-md); font-weight: var(--fw-semibold); background: var(--surface-muted); border: 1px solid var(--border-light); color: var(--text-tertiary); }
.db-status-bar.connected { background: var(--success-bg); border-color: var(--success-border); color: var(--success-text); }
.db-status-dot { width: 10px; height: 10px; border-radius: 50%; background: var(--color-offline); flex-shrink: 0; }
.db-status-bar.connected .db-status-dot { background: var(--color-online); box-shadow: 0 0 0 4px rgba(47,143,99,0.12); }
.db-actions { display: flex; gap: 8px; margin-top: 6px; }
.db-test-result { margin-top: 10px; padding: 10px 12px; border-radius: var(--radius-md); font-size: var(--fs-sm); font-weight: var(--fw-semibold); }
.db-test-result.ok { background: var(--success-bg); color: var(--success-text); border: 1px solid var(--success-border); }
.db-test-result.err { background: var(--danger-bg); color: var(--danger-text); border: 1px solid var(--danger-border); }
.status-table { display: flex; flex-direction: column; }
.status-row { display: flex; justify-content: space-between; align-items: center; min-height: 42px; padding: 8px 0; font-size: var(--fs-md); border-bottom: 1px solid var(--border-light); }
.status-row span:first-child { color: var(--text-tertiary); }
.status-val { display: flex; align-items: center; gap: 5px; font-weight: var(--fw-medium); }
.status-val.on { color: var(--success-text); }
.status-val.off { color: var(--text-placeholder); }
.s-dot { width: 7px; height: 7px; border-radius: 50%; background: var(--text-placeholder); }
.status-val.on .s-dot { background: var(--color-online); box-shadow: 0 0 0 3px rgba(47,143,99,0.12); }
.status-val.off .s-dot { background: var(--color-offline); }
.network-card {
  grid-column: span 2;
}
.network-hint,
.network-note,
.network-empty {
  color: var(--text-muted);
  font-size: var(--fs-sm);
  line-height: 1.6;
}
.network-url-list {
  display: flex;
  flex-direction: column;
  gap: 8px;
  margin: 12px 0;
}
.network-url-row {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 10px 12px;
  border: 1px solid var(--border-light);
  border-radius: var(--radius-md);
  background: var(--surface-muted);
}
.network-url-row code {
  flex: 1;
  min-width: 0;
  overflow-wrap: anywhere;
  font-family: var(--font-mono);
  color: var(--text-secondary);
}
.network-note {
  padding: 10px 12px;
  border-radius: var(--radius-md);
  background: var(--info-bg);
  border: 1px solid var(--info-border);
  color: var(--info-text);
}

@media (max-width: 900px) {
  .sv-grid { grid-template-columns: 1fr; }
  .network-card { grid-column: auto; }
}
@media (max-width: 760px) {
  .settings-view { padding: 16px 12px; }
  .sv-header { align-items: flex-start; flex-direction: column; gap: 4px; }
  .sv-card { padding: 18px; }
  .db-actions { flex-direction: column; }
}
</style>
