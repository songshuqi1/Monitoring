<template>
  <div class="dialog-overlay" @click.self="$emit('close')">
    <div class="dialog project-form-dialog">
      <h3>{{ mode === 'edit' ? '编辑项目' : '创建项目' }}</h3>
      <form @submit.prevent="handleSubmit">
        <div class="form-group" :class="{ 'is-error': errors.name }">
          <label>项目名称</label>
          <input v-model.trim="form.name" maxlength="40" placeholder="例如：一号产线监控" />
          <p v-if="errors.name" class="field-error">{{ errors.name }}</p>
        </div>

        <div class="form-group" :class="{ 'is-error': errors.description }">
          <label>描述</label>
          <textarea v-model.trim="form.description" maxlength="160" rows="3" placeholder="填写项目用途、产线或班组信息"></textarea>
          <p v-if="errors.description" class="field-error">{{ errors.description }}</p>
        </div>

        <label v-if="mode === 'edit'" class="layout-option">
          <input type="checkbox" v-model="useCurrentLayout" />
          <span>用当前画布覆盖此项目布局</span>
        </label>

        <div class="form-summary">
          <span>将保存 {{ layoutWidgetCount(effectiveLayout) }} 个组件 / {{ layoutConnectionCount(effectiveLayout) }} 条连接线</span>
        </div>

        <div class="dialog-actions">
          <button type="button" class="btn btn-secondary" @click="$emit('close')">取消</button>
          <button type="submit" class="btn btn-primary" :disabled="submitting">
            {{ submitting ? '保存中...' : '保存' }}
          </button>
        </div>
      </form>
    </div>
  </div>
</template>

<script setup>
import { computed, reactive, ref } from 'vue'

const props = defineProps({
  mode: { type: String, default: 'create' },
  project: { type: Object, default: null },
  currentLayout: { type: [Array, Object], default: () => ({ widgets: [], connections: [] }) },
  submitting: { type: Boolean, default: false }
})

const emit = defineEmits(['close', 'submit'])

const form = reactive({
  name: props.project?.name || '',
  description: props.project?.description || ''
})
const errors = reactive({})
const useCurrentLayout = ref(props.mode === 'create')
let lastSubmitAt = 0

const effectiveLayout = computed(() => {
  if (props.mode === 'edit' && !useCurrentLayout.value) return props.project?.layout || []
  return props.currentLayout
})

function layoutWidgetCount(layout) {
  if (Array.isArray(layout)) return layout.length
  return Array.isArray(layout?.widgets) ? layout.widgets.length : 0
}

function layoutConnectionCount(layout) {
  return Array.isArray(layout?.connections) ? layout.connections.length : 0
}

function validate() {
  errors.name = ''
  errors.description = ''
  if (!form.name.trim()) errors.name = '请输入项目名称'
  else if (form.name.trim().length > 40) errors.name = '项目名称不能超过 40 个字符'
  if (form.description.length > 160) errors.description = '项目描述不能超过 160 个字符'
  return !errors.name && !errors.description
}

function handleSubmit() {
  const current = Date.now()
  if (current - lastSubmitAt < 600 || props.submitting) return
  lastSubmitAt = current
  if (!validate()) return
  emit('submit', {
    name: form.name.trim(),
    description: form.description.trim(),
    layout: effectiveLayout.value
  })
}
</script>

<style scoped>
.project-form-dialog {
  width: 480px;
}
.layout-option {
  display: flex;
  align-items: center;
  gap: 8px;
  min-height: 36px;
  margin-top: -4px;
  color: var(--text-secondary);
  font-size: var(--fs-sm);
  font-weight: var(--fw-semibold);
}
.layout-option input {
  width: 16px;
  height: 16px;
  min-height: 0;
  accent-color: var(--accent);
}
.form-summary {
  margin-top: 12px;
  padding: 10px 12px;
  border-radius: var(--radius-md);
  border: 1px solid var(--border-light);
  background: var(--surface-muted);
  color: var(--text-tertiary);
  font-size: var(--fs-xs);
  font-family: var(--font-mono);
}
</style>
