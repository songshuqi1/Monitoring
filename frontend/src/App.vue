<template>
  <!-- 登录页面不显示主框架 -->
  <router-view v-if="$route.path === '/login'" />
  <div v-else class="app-shell">
    <!-- 顶部工具栏 -->
    <header class="app-toolbar">
      <div class="toolbar-brand">
        <svg viewBox="0 0 40 40" width="22" height="22">
          <rect x="4" y="4" width="32" height="32" rx="6" fill="#4f6fb8"/>
          <path d="M14 28V12l16 8-16 8z" fill="white"/>
        </svg>
        <span class="brand-text">监控平台</span>
      </div>
      <button class="mobile-menu-btn" :class="{ open: mobileMenuOpen }" @click="mobileMenuOpen = !mobileMenuOpen" title="菜单">
        <span></span>
        <span></span>
        <span></span>
      </button>

      <nav class="toolbar-nav" :class="{ open: mobileMenuOpen }">
        <router-link v-for="item in navItems" :key="item.path" :to="item.path"
          class="toolbar-tab" :class="{ active: $route.path === item.path }" @click="mobileMenuOpen = false">
          <span v-if="item.icon" class="tab-icon" v-html="item.icon"></span>
          <span>{{ item.label }}</span>
        </router-link>
        <div v-if="$route.path === '/canvas'" class="view-switch" aria-label="监控视图切换">
          <button
            type="button"
            class="view-switch-tab"
            :class="{ active: store.canvasViewMode === '2d' }"
            @click="store.canvasViewMode = '2d'"
          >
            2D
          </button>
          <button
            type="button"
            class="view-switch-tab"
            :class="{ active: store.canvasViewMode === '3d' }"
            @click="store.canvasViewMode = '3d'"
          >
            3D
          </button>
        </div>
      </nav>

      <div class="toolbar-right">
        <span class="user-badge" title="当前登录用户">
          <span class="user-icon">👤</span>
          {{ authUsername }}
        </span>
        <button class="btn-logout" @click="handleLogout" title="退出登录">退出</button>
        <span class="conn-status" :class="{ connected: store.wsConnected }">
          <span class="conn-dot"></span>
          {{ store.wsConnected ? '已连接' : '未连接' }}
        </span>
        <span class="online-count">在线: {{ store.onlineCount }}</span>
      </div>
    </header>

    <!-- 主内容区 -->
    <main class="app-main">
      <router-view />
    </main>

    <!-- 底部状态栏 -->
    <footer class="app-statusbar">
      <span v-if="store.layoutStorageError" role="alert" style="color: var(--danger-text)">{{ store.layoutStorageError }}</span>
      <div class="status-left">
        <span class="status-item">
          <span class="s-dot" :class="store.systemStatus.opcuaServer ? 'on' : ''"></span>
          OPC UA {{ store.systemStatus.opcuaServer ? '运行中' : '已停止' }}
        </span>
        <span class="status-item">
          <span class="s-dot" :class="store.systemStatus.database ? 'on' : 'off'"></span>
          <span class="db-label" :class="store.systemStatus.database ? 'on' : 'off'">DB</span>
          {{ store.systemStatus.database ? '已连接' : '未连接' }}
        </span>
      </div>
      <div class="status-right">
        <span class="status-time">{{ clock }}</span>
      </div>
    </footer>
  </div>
</template>

<script setup>
import { ref, onMounted, onUnmounted, watch } from 'vue'
import { useRouter, useRoute } from 'vue-router'
import { useMonitorStore } from './store/index.js'
import { useProjectStore } from './store/projectStore.js'
import api from './api/index.js'

const router = useRouter()
const route = useRoute()
const store = useMonitorStore()
const projectStore = useProjectStore()
const clock = ref('')
const mobileMenuOpen = ref(false)
const authUsername = ref(localStorage.getItem('auth_username') || 'admin')

const navItems = [
  { path: '/canvas',   label: '监控画布', icon: '&#9632;' },
  { path: '/comm',     label: '通信资源', icon: '&#9741;' },
  { path: '/parameters', label: '参数下发', icon: '&#8595;' },
  { path: '/data',     label: '数据视图', icon: '&#9776;' },
  { path: '/settings', label: '系统设置', icon: '&#9881;' },
  { path: '/component-config', label: '组件配置', icon: '' },
]

const handleLogout = async () => {
  try {
    await api.authLogout()
  } catch (e) {
    // ignore
  }
  localStorage.removeItem('auth_token')
  localStorage.removeItem('auth_username')
  layoutLoaded = false
  runtimeStarted = false
  router.replace('/login')
}

let clockTimer = null
let statusTimer = null
let realtimeTimer = null
let residentTimer = null
let layoutLoaded = false
let runtimeStarted = false

function isAuthenticated() {
  return !!localStorage.getItem('auth_token')
}

function startDataLoading() {
  if (!layoutLoaded) {
    store.loadLayout()
    layoutLoaded = true
  }
  store.loadResidentData()
  if (!runtimeStarted) {
    store.connectWebSocket()
    store.startTrendSampler()
    projectStore.fetchProjects().catch(() => {})
    projectStore.restoreCurrentProject().then(project => {
      if (project && store.draftProjectId !== project.id) store.activateProject(project)
    }).catch(() => {})
    runtimeStarted = true
  }
  store.fetchStatus()
}

// 监听路由变化：登录成功后重新加载数据
watch(() => route.path, (newPath) => {
  if (newPath !== '/login' && isAuthenticated()) {
    authUsername.value = localStorage.getItem('auth_username') || 'admin'
    startDataLoading()
  }
})

onMounted(() => {
  // 只有已登录时才加载数据
  if (isAuthenticated()) {
    startDataLoading()
  }

  const update = () => {
    const d = new Date()
    clock.value = d.toLocaleString('zh-CN', { hour12: false,
      hour: '2-digit', minute: '2-digit', second: '2-digit' })
  }
  update()
  clockTimer = setInterval(update, 1000)
  realtimeTimer = setInterval(() => {
    if (isAuthenticated()) {
      store.tickDataClock()
      store.loadRealtime()
    }
  }, 1000)
  residentTimer = setInterval(() => {
    if (isAuthenticated()) {
      store.refreshResidentMetadata()
    }
  }, 3000)
  statusTimer = setInterval(() => {
    if (isAuthenticated()) {
      store.fetchStatus()
    }
  }, 10000)
})

onUnmounted(() => {
  store.stopTrendSampler()
  store.disconnectWebSocket()
  if (clockTimer) clearInterval(clockTimer)
  if (statusTimer) clearInterval(statusTimer)
  if (realtimeTimer) clearInterval(realtimeTimer)
  if (residentTimer) clearInterval(residentTimer)
})
</script>

<style scoped>
.app-shell {
  display: flex;
  flex-direction: column;
  height: 100vh;
  background: var(--bg-secondary);
}

/* ---- 顶部工具栏 ---- */
.app-toolbar {
  height: var(--header-height);
  background: var(--bg-header);
  border-bottom: 1px solid var(--border-light);
  display: flex;
  align-items: center;
  padding: 0 18px;
  gap: 22px;
  flex-shrink: 0;
  box-shadow: var(--shadow-xs);
  backdrop-filter: blur(12px);
  -webkit-app-region: drag;
}

.toolbar-brand {
  display: flex;
  align-items: center;
  gap: 10px;
  -webkit-app-region: no-drag;
}

.mobile-menu-btn {
  display: none;
  width: 40px;
  height: 40px;
  align-items: center;
  justify-content: center;
  flex-direction: column;
  gap: 5px;
  border-radius: var(--radius-sm);
  border: 1px solid var(--border-light);
  background: var(--bg-primary);
  -webkit-app-region: no-drag;
}
.mobile-menu-btn span {
  width: 18px;
  height: 2px;
  border-radius: var(--radius-full);
  background: var(--text-secondary);
  transition: transform var(--transition-fast), opacity var(--transition-fast);
}
.mobile-menu-btn.open span:nth-child(1) { transform: translateY(7px) rotate(45deg); }
.mobile-menu-btn.open span:nth-child(2) { opacity: 0; }
.mobile-menu-btn.open span:nth-child(3) { transform: translateY(-7px) rotate(-45deg); }
.brand-text {
  font-size: var(--fs-xl);
  font-weight: var(--fw-bold);
  color: var(--text-primary);
  letter-spacing: 0;
}

.toolbar-nav {
  display: flex;
  align-items: center;
  gap: 4px;
  -webkit-app-region: no-drag;
}

.toolbar-tab {
  display: flex;
  align-items: center;
  gap: 7px;
  min-height: 34px;
  padding: 0 12px;
  border-radius: var(--radius-sm);
  color: var(--text-tertiary);
  font-size: var(--fs-sm);
  font-weight: var(--fw-semibold);
  text-decoration: none;
  transition: background var(--transition-fast), color var(--transition-fast), box-shadow var(--transition-fast);
}
.toolbar-tab:hover {
  background: var(--bg-hover);
  color: var(--text-primary);
  text-decoration: none;
}
.toolbar-tab.active {
  background: var(--accent-soft);
  color: var(--accent);
  box-shadow: inset 0 0 0 1px var(--info-border);
}
.tab-icon { font-size: 13px; opacity: 0.8; }

.view-switch {
  display: flex;
  align-items: center;
  min-height: 30px;
  padding: 2px;
  border: 1px solid var(--border-light);
  border-radius: var(--radius-sm);
  background: var(--bg-primary);
  overflow: hidden;
}

.view-switch-tab {
  min-width: 38px;
  min-height: 26px;
  padding: 0 10px;
  border: 0;
  border-radius: calc(var(--radius-sm) - 2px);
  background: transparent;
  color: var(--text-tertiary);
  font-size: var(--fs-xs);
  font-weight: var(--fw-semibold);
  cursor: pointer;
}

.view-switch-tab.active {
  background: var(--accent);
  color: var(--text-inverse);
}

.toolbar-right {
  margin-left: auto;
  display: flex;
  align-items: center;
  gap: 10px;
  -webkit-app-region: no-drag;
}

.conn-status {
  display: flex;
  align-items: center;
  gap: 6px;
  min-height: 28px;
  font-size: var(--fs-xs);
  font-weight: var(--fw-semibold);
  color: var(--text-tertiary);
  padding: 0 10px;
  background: var(--surface-muted);
  border-radius: var(--radius-full);
  border: 1px solid var(--border-light);
}
.conn-dot {
  width: 7px; height: 7px;
  border-radius: 50%;
  background: var(--color-offline);
}
.conn-status.connected .conn-dot {
  background: var(--color-online);
  box-shadow: 0 0 0 3px rgba(47,143,99,0.12);
}
.conn-status.connected {
  color: var(--color-online);
}

.online-count {
  min-height: 28px;
  display: inline-flex;
  align-items: center;
  padding: 0 10px;
  border-radius: var(--radius-full);
  background: var(--surface-muted);
  border: 1px solid var(--border-light);
  font-size: var(--fs-xs);
  color: var(--text-tertiary);
  font-family: var(--font-mono);
}

.user-badge {
  display: flex;
  align-items: center;
  gap: 4px;
  min-height: 28px;
  padding: 0 10px;
  border-radius: var(--radius-full);
  background: rgba(79,111,184,0.12);
  border: 1px solid rgba(79,111,184,0.25);
  font-size: var(--fs-xs);
  color: #8ba4d6;
  font-weight: var(--fw-medium);
}
.user-icon { font-size: 13px; }

.btn-logout {
  min-height: 28px;
  padding: 0 12px;
  border-radius: var(--radius-full);
  background: transparent;
  border: 1px solid var(--border-light);
  color: var(--text-tertiary);
  font-size: var(--fs-xs);
  cursor: pointer;
  transition: all 0.15s;
}
.btn-logout:hover {
  background: rgba(255,77,77,0.1);
  border-color: rgba(255,77,77,0.3);
  color: #ff6b6b;
}

/* ---- 主内容区 ---- */
.app-main {
  flex: 1;
  overflow: hidden;
}

#three-container {
  width: 100%;
  height: 100%;
  background: #0a0a0a;
  color: #fff;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: var(--fs-xl);
  font-weight: var(--fw-semibold);
}

/* ---- 底部状态栏 ---- */
.app-statusbar {
  height: var(--statusbar-height);
  background: var(--bg-primary);
  border-top: 1px solid var(--border-light);
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 0 16px;
  font-size: var(--fs-xs);
  color: var(--text-tertiary);
  flex-shrink: 0;
  user-select: none;
}
.status-left {
  display: flex;
  align-items: center;
  gap: 16px;
}
.status-item {
  display: flex;
  align-items: center;
  gap: 5px;
}
.s-dot {
  width: 7px; height: 7px;
  border-radius: 50%;
  background: var(--text-placeholder);
}
.s-dot.on {
  background: var(--color-online);
  box-shadow: 0 0 0 3px rgba(47,143,99,0.12);
}
.s-dot.off {
  background: var(--color-offline);
}
.db-label {
  display: inline-block;
  padding: 2px 7px;
  border-radius: var(--radius-full);
  font-size: 13px;
  font-weight: var(--fw-semibold);
  line-height: 1;
}
.db-label.on {
  background: var(--success-bg);
  color: var(--success-text);
}
.db-label.off {
  background: var(--danger-bg);
  color: var(--danger-text);
}
.status-right {
  font-family: var(--font-mono);
}
.status-time { color: var(--text-tertiary); }


/* Main shell readability: keep the runtime toolbar comfortable on large screens. */
.app-toolbar {
  font-size: 14px;
}
.toolbar-tab {
  font-size: 14px;
  min-height: 36px;
}
.tab-icon,
.user-icon {
  font-size: 14px;
}
.view-switch-tab,
.conn-status,
.online-count,
.user-badge,
.btn-logout {
  font-size: 13px;
}
.app-statusbar {
  font-size: 13px;
}
.db-label {
  font-size: 13px;
}
@media (max-width: 760px) {
  .app-toolbar {
    gap: 10px;
    padding: 0 10px;
    position: relative;
  }
  .brand-text { display: inline; font-size: var(--fs-lg); }
  .mobile-menu-btn { display: inline-flex; }
  .toolbar-nav {
    position: absolute;
    top: calc(var(--header-height) - 4px);
    left: 10px;
    right: 10px;
    display: none;
    flex-direction: column;
    align-items: stretch;
    gap: 4px;
    padding: 8px;
    border: 1px solid var(--border-light);
    border-radius: var(--radius-lg);
    background: var(--bg-primary);
    box-shadow: var(--shadow-lg);
    z-index: 20;
  }
  .toolbar-nav.open { display: flex; }
  .toolbar-tab {
    width: 100%;
    min-height: 44px;
    justify-content: flex-start;
    padding: 0 12px;
  }
  .view-switch {
    align-self: flex-end;
  }
  .toolbar-right { gap: 6px; }
  .online-count { display: none; }
}
</style>
