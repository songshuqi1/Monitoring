<template>
  <CanvasPage v-if="store.canvasViewMode === '2d'" />
  <Canvas3DPage v-else />
</template>

<script setup>
import { defineAsyncComponent } from 'vue'
import { useMonitorStore } from '../store/index.js'
import CanvasPage from './CanvasPage.vue'
const Canvas3DPage = defineAsyncComponent(() => import('./Canvas3DPage.vue'))
const store = useMonitorStore()
</script>
