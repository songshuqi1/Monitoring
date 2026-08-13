<template>
  <div class="login-container">
    <div class="login-card">
      <div class="login-header">
        <svg viewBox="0 0 40 40" width="48" height="48">
          <rect x="4" y="4" width="32" height="32" rx="6" fill="#4f6fb8"/>
          <path d="M14 28V12l16 8-16 8z" fill="white"/>
        </svg>
        <h1>监控平台</h1>
        <p>Monitoring Platform</p>
      </div>
      <form class="login-form" @submit.prevent="handleLogin">
        <div class="form-group">
          <label>账号</label>
          <input
            v-model.trim="username"
            type="text"
            placeholder="请输入账号"
            autocomplete="username"
            :disabled="loading"
          />
        </div>
        <div class="form-group">
          <label>密码</label>
          <input
            v-model="password"
            type="password"
            placeholder="请输入密码"
            autocomplete="current-password"
            :disabled="loading"
          />
        </div>
        <div v-if="errorMsg" class="login-error">{{ errorMsg }}</div>
        <button type="submit" class="btn btn-primary login-btn" :disabled="loading">
          {{ loading ? '登录中...' : '登 录' }}
        </button>
      </form>
    </div>
  </div>
</template>

<script>
import { ref } from 'vue'
import { useRouter } from 'vue-router'
import api from '../api/index.js'

export default {
  name: 'LoginView',
  setup() {
    const router = useRouter()
    const username = ref('admin')
    const password = ref('sapi.1992')
    const loading = ref(false)
    const errorMsg = ref('')

    const handleLogin = async () => {
      errorMsg.value = ''
      if (!username.value || !password.value) {
        errorMsg.value = '请输入账号和密码'
        return
      }
      loading.value = true
      try {
        const res = await api.authLogin(username.value, password.value)
        if (res.data && res.data.token) {
          localStorage.setItem('auth_token', res.data.token)
          localStorage.setItem('auth_username', res.data.username)
          router.replace('/canvas')
        }
      } catch (e) {
        const msg = e?.response?.data?.message || '登录失败，请检查账号密码'
        errorMsg.value = msg
      } finally {
        loading.value = false
      }
    }

    return { username, password, loading, errorMsg, handleLogin }
  }
}
</script>

<style scoped>
.login-container {
  display: flex;
  align-items: center;
  justify-content: center;
  min-height: 100vh;
  background: linear-gradient(135deg, #1a1a2e 0%, #16213e 50%, #0f3460 100%);
}
.login-card {
  background: rgba(255,255,255,0.05);
  backdrop-filter: blur(12px);
  border: 1px solid rgba(255,255,255,0.1);
  border-radius: 16px;
  padding: 48px 40px;
  width: 400px;
  max-width: 90vw;
  box-shadow: 0 25px 60px rgba(0,0,0,0.4);
}
.login-header {
  text-align: center;
  margin-bottom: 36px;
}
.login-header svg {
  margin-bottom: 12px;
}
.login-header h1 {
  font-size: 24px;
  font-weight: 700;
  color: #e0e0e0;
  margin: 0 0 4px;
}
.login-header p {
  font-size: 13px;
  color: #8892b0;
  margin: 0;
}
.login-form .form-group {
  margin-bottom: 20px;
}
.login-form label {
  display: block;
  font-size: 13px;
  color: #a8b2d1;
  margin-bottom: 6px;
  font-weight: 500;
}
.login-form input {
  width: 100%;
  padding: 12px 16px;
  border: 1px solid rgba(255,255,255,0.12);
  border-radius: 8px;
  background: rgba(255,255,255,0.06);
  color: #e0e0e0;
  font-size: 15px;
  outline: none;
  transition: border-color 0.2s;
  box-sizing: border-box;
}
.login-form input:focus {
  border-color: #4f6fb8;
  background: rgba(255,255,255,0.08);
}
.login-form input::placeholder {
  color: #5a6a8a;
}
.login-error {
  background: rgba(255,77,77,0.12);
  border: 1px solid rgba(255,77,77,0.3);
  color: #ff6b6b;
  padding: 10px 14px;
  border-radius: 8px;
  font-size: 13px;
  margin-bottom: 16px;
  text-align: center;
}
.login-btn {
  width: 100%;
  padding: 13px;
  font-size: 16px;
  font-weight: 600;
  border-radius: 8px;
  cursor: pointer;
  transition: all 0.2s;
}
.login-btn:disabled {
  opacity: 0.6;
  cursor: not-allowed;
}
</style>
