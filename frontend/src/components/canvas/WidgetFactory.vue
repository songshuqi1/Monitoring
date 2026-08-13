<template>
  <TrendChartWidget v-if="widget.type === 'trendChart'"     :widget="widget" />
  <DigitalDisplayWidget v-else-if="widget.type === 'digitalDisplay'" :widget="widget" />
  <GaugeWidget v-else-if="widget.type === 'gauge'"          :widget="widget" />
  <ButtonWidget v-else-if="widget.type === 'button'"        :widget="widget" />
  <AlarmListWidget v-else-if="widget.type === 'alarmList'"  :widget="widget" />
  <IndicatorWidget v-else-if="widget.type === 'indicator'"  :widget="widget" />
  <StatusCircleWidget v-else-if="widget.type === 'statusCircle'" :widget="widget" />
  <LabelWidget v-else-if="widget.type === 'label'"          :widget="widget" />
  <FrameBoxWidget v-else-if="widget.type === 'frameBox'"    :widget="widget" />
  <CustomShapeWidget v-else-if="widget.type === 'customShape'" :widget="widget" />
  <ProcessValueTagWidget v-else-if="widget.type === 'processValueTag'" :widget="widget" />
  <ProcessDeviceWidget v-else-if="widgetType.startsWith('process')" :widget="widget" :readonly="readonly" />
  <div v-else class="unknown-widget">
    <span>未知组件: {{ widgetType || '未命名' }}</span>
  </div>
</template>

<script setup>
import { computed } from 'vue'
import TrendChartWidget from '../widgets/TrendChartWidget.vue'
import DigitalDisplayWidget from '../widgets/DigitalDisplayWidget.vue'
import GaugeWidget from '../widgets/GaugeWidget.vue'
import ButtonWidget from '../widgets/ButtonWidget.vue'
import AlarmListWidget from '../widgets/AlarmListWidget.vue'
import IndicatorWidget from '../widgets/IndicatorWidget.vue'
import StatusCircleWidget from '../widgets/StatusCircleWidget.vue'
import LabelWidget from '../widgets/LabelWidget.vue'
import FrameBoxWidget from '../widgets/FrameBoxWidget.vue'
import CustomShapeWidget from '../widgets/CustomShapeWidget.vue'
import ProcessDeviceWidget from '../widgets/ProcessDeviceWidget.vue'
import ProcessValueTagWidget from '../widgets/ProcessValueTagWidget.vue'

const props = defineProps({
  widget: { type: Object, required: true },
  readonly: { type: Boolean, default: false }
})

const widgetType = computed(() => String(props.widget?.type || ''))
</script>

<style scoped>
.unknown-widget {
  display: flex;
  align-items: center;
  justify-content: center;
  height: 100%;
  color: var(--text-muted);
  font-size: var(--fs-sm);
}
</style>
