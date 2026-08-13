import { computed, ref } from 'vue'

export function useRequest(requester) {
  const loading = ref(false)
  const error = ref(null)
  const data = ref(null)
  const success = ref(false)
  let requestSeq = 0

  async function run(...args) {
    const seq = ++requestSeq
    loading.value = true
    error.value = null
    success.value = false

    try {
      const result = await requester(...args)
      if (seq !== requestSeq) return data.value
      data.value = result
      success.value = true
      return result
    } catch (err) {
      if (seq !== requestSeq) return data.value
      error.value = normalizeError(err)
      throw err
    } finally {
      if (seq === requestSeq) loading.value = false
    }
  }

  function reset() {
    requestSeq++
    loading.value = false
    error.value = null
    data.value = null
    success.value = false
  }

  return {
    data,
    loading,
    error,
    success,
    errorMessage: computed(() => error.value?.message || ''),
    run,
    reset
  }
}

export function normalizeError(err) {
  if (!err) return { message: '未知错误' }
  if (typeof err === 'string') return { message: err }
  if (err.userMessage) return { message: err.userMessage, code: err.code }
  if (err.response?.data?.message) return { message: err.response.data.message, code: err.response.status }
  if (err.message) return { message: err.message, code: err.code }
  return { message: '请求失败' }
}
