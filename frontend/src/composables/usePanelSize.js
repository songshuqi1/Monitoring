import { ref, watch } from 'vue'

export function usePanelSize(key, fallback) {
  let initial = fallback
  try {
    const saved = localStorage.getItem(key)
    if (saved != null && Number.isFinite(Number(saved)) && Number(saved) > 0) initial = Number(saved)
  } catch { /* Resizing still works when local storage is unavailable. */ }
  const size = ref(initial)
  watch(size, value => {
    try { localStorage.setItem(key, String(value)) } catch { /* Keep the current session's size. */ }
  })
  return size
}
