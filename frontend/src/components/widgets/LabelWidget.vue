<template>
  <div class="lw" :style="labelStyle">{{ widget.config.text || widget.config.title || '标签' }}</div>
</template>

<script setup>
import { computed } from 'vue'
const props = defineProps({ widget: { type: Object, required: true } })
const horizontalMap = {
  left: 'flex-start',
  center: 'center',
  right: 'flex-end'
}
const verticalMap = {
  top: 'flex-start',
  center: 'center',
  bottom: 'flex-end'
}
const labelStyle = computed(() => ({
  fontSize: (props.widget.config.fontSize || 13) + 'px',
  color: props.widget.config.color || 'var(--text-secondary)',
  fontWeight: props.widget.config.bold ? '700' : '400',
  fontStyle: props.widget.config.italic ? 'italic' : 'normal',
  textDecoration: props.widget.config.underline ? 'underline' : 'none',
  textAlign: props.widget.config.align || 'center',
  justifyContent: horizontalMap[props.widget.config.align] || 'center',
  alignItems: verticalMap[props.widget.config.verticalAlign] || 'center',
  background: props.widget.config.background || 'transparent',
  borderColor: props.widget.config.borderColor || 'transparent',
  borderWidth: (props.widget.config.borderWidth || 0) + 'px',
  borderRadius: (props.widget.config.borderRadius || 0) + 'px',
}))
</script>

<style scoped>
.lw {
  display: flex;
  width: 100%; height: 100%; padding: 4px 8px;
  box-sizing: border-box;
  border-style: solid;
  overflow: hidden;
  white-space: pre-line;
  text-overflow: ellipsis;
  line-height: 1.35;
  color: var(--text-secondary);
}
</style>
