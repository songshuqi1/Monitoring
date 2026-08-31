import axios from 'axios'

// 创建 axios 实例
const http = axios.create({
  baseURL: '/api',
  timeout: 10000,
  headers: {
    'Content-Type': 'application/json'
  }
})

// 请求拦截器：自动附加 Authorization token
http.interceptors.request.use(config => {
  const token = localStorage.getItem('auth_token')
  if (token) {
    config.headers.Authorization = `Bearer ${token}`
  }
  return config
})

function isExpectedApiError(error) {
  const message = String(error?.message || '').toLowerCase()
  return Boolean(
    error?.config?.silent ||
    error?.code === 'ERR_CANCELED' ||
    error?.code === 'ECONNABORTED' ||
    message.includes('aborted') ||
    message.includes('canceled') ||
    message.includes('cancelled')
  )
}

// 响应拦截器
http.interceptors.response.use(
  response => response,
  error => {
    if (error.response && error.response.status === 401) {
      // 未授权，清除本地凭证
      localStorage.removeItem('auth_token')
      localStorage.removeItem('auth_username')
    }
    if (!isExpectedApiError(error)) {
      console.warn('API request failed:', error.message)
    }
    return Promise.reject(error)
  }
)

const api = {
  // ---- 认证 ----
  authLogin(username, password) {
    return http.post('/auth/login', { username, password })
  },
  authLogout() {
    return http.post('/auth/logout')
  },
  authCheck() {
    return http.get('/auth/check')
  },

  // ---- 变量定义 ----
  getVariables() {
    return http.get('/variables')
  },
  getVariable(id) {
    return http.get(`/variables/${id}`)
  },
  createVariable(data) {
    return http.post('/variables', data)
  },
  updateVariable(id, data) {
    return http.put(`/variables/${id}`, data)
  },
  deleteVariable(id) {
    return http.delete(`/variables/${id}`)
  },
  bulkDeleteVariables(mode = 'all', ids = []) {
    return http.post('/variables/bulk-delete', { mode, ids })
  },

  // ---- 实时数据 ----
  getRealtime() {
    return http.get('/realtime')
  },

  // ---- 历史数据 ----
  getHistory(varId, from = 0, to = 0) {
    let url = `/history/${varId}`
    const params = []
    if (from) params.push(`from=${from}`)
    if (to) params.push(`to=${to}`)
    if (params.length) url += '?' + params.join('&')
    return http.get(url)
  },

  getHistoryDb(varId, from = 0, to = 0) {
    let url = `/historydb/${varId}`
    const params = []
    if (from) params.push(`from=${from}`)
    if (to) params.push(`to=${to}`)
    if (params.length) url += '?' + params.join('&')
    return http.get(url)
  },

  getHistoryDbWide(from = 0, to = 0, max = 5000) {
    let url = '/historydb/wide'
    const params = []
    if (from) params.push(`from=${from}`)
    if (to) params.push(`to=${to}`)
    if (max) params.push(`max=${max}`)
    if (params.length) url += '?' + params.join('&')
    return http.get(url)
  },

  // ---- 写入变量 ----
  writeVariable(varId, value) {
    return http.post(`/write/${varId}`, { value })
  },

  // ---- 下发审计记录 ----
  getWriteAudit() {
    return http.get('/write-audit')
  },
  clearWriteAudit() {
    return http.delete('/write-audit')
  },

  // ---- 报警 ----
  getAlarms() {
    return http.get('/alarms')
  },
  acknowledgeAlarm(id) {
    return http.post(`/alarms/${id}/ack`)
  },

  // ---- 日志 ----
  getLogs() {
    return http.get('/logs')
  },

  // ---- 配置 ----
  getConfig(key) {
    return http.get(`/config/${key}`)
  },
  setConfig(key, value) {
    return http.post(`/config/${key}`, { value })
  },

  // ---- 状态 ----
  getStatus() {
    return http.get('/status')
  },

  getNetworkUrls() {
    return http.get('/network/urls')
  },

  // ---- 自定义组件库 ----
  getCustomComponents() {
    return http.get('/custom-components')
  },
  saveCustomComponents(components) {
    return http.post('/custom-components', components)
  },

  // ---- 数据库测试 ----
  dbTest() {
    return http.get('/db/test')
  },

  // ---- 数据库连接 ----
  dbConnect(host, port, user, pass, dbname) {
    return http.post('/db/connect', { host, port, user, pass, dbname })
  },

  // ---- 通用 POST ----  用于通信资源采集控制等
  post(path, data) {
    return http.post(path, data)
  },

  // ---- OPC UA 连接 ----
  connectOpcua(url) {
    return http.post('/opcua/connect', { url })
  },

  // ---- UDP 采集控制 ----
  udpStart(address, port) {
    return http.post('/udp/start', { address: address || '0.0.0.0', port })
  },
  udpStop() {
    return http.post('/udp/stop', {})
  },

  testSdc(sdcUrl) {
    return http.post('/sdc/test', { sdcUrl })
  },
  testModbus(config) {
    return http.post('/modbus/test', config, { timeout: 6000 })
  },

  // ---- 软件定义通信资源（后端持久化 + 多任务） ----
  getCommunicationResources() {
    return http.get('/communication/resources')
  },
  saveCommunicationResource(resource) {
    return http.post('/communication/resources', resource)
  },
  deleteCommunicationResource(id) {
    return http.delete(`/communication/resources/${id}`)
  },
  startCommunicationResource(id) {
    return http.post(`/communication/resources/${id}/start`)
  },
  stopCommunicationResource(id) {
    return http.post(`/communication/resources/${id}/stop`)
  },
  getCommunicationStatus() {
    return http.get('/communication/status')
  },

  // ---- 组件导入/导出 ----
  exportCustomComponents(name) {
    return http.get('/custom-components/export', {
      params: name ? { name } : {},
      responseType: 'blob'
    })
  },
  importCustomComponents(payload) {
    return http.post('/custom-components/import', payload)
  }
}

export default api
