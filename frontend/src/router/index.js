import { createRouter, createWebHistory } from 'vue-router'
import LoginView from '../views/LoginView.vue'
import CanvasPage from '../views/CanvasPage.vue'
import CommunicationView from '../views/CommunicationView.vue'
import DataView from '../views/DataView.vue'
import SettingsView from '../views/SettingsView.vue'
import ComponentDesignerView from '../components/custom/ComponentDesignerDialog.vue'

const routes = [
  { path: '/login', name: 'Login', component: LoginView, meta: { title: '登录', public: true } },
  { path: '/', redirect: '/canvas' },
  { path: '/canvas',  name: 'Canvas',   component: CanvasPage,   meta: { title: '监控画布' } },
  { path: '/comm',    name: 'Comm',     component: CommunicationView, meta: { title: '通信资源' } },
  { path: '/data',    name: 'Data',     component: DataView,     meta: { title: '数据视图' } },
  { path: '/settings', name: 'Settings', component: SettingsView, meta: { title: '系统设置' } },
  { path: '/component-config', name: 'ComponentConfig', component: ComponentDesignerView, meta: { title: '组件配置' } },
]

const router = createRouter({ history: createWebHistory(), routes })

// 路由守卫：未登录用户重定向到登录页
router.beforeEach((to, from, next) => {
  const token = localStorage.getItem('auth_token')
  if (to.meta.public) {
    // 公开页面（登录页），已登录用户重定向到主页
    if (token && to.path === '/login') {
      next('/canvas')
    } else {
      next()
    }
  } else {
    // 其他页面需要登录
    if (!token) {
      next('/login')
    } else {
      next()
    }
  }
})

export default router
