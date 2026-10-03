<script setup lang="ts">
import { computed, ref, watch } from 'vue';
import { t } from '../i18n';
import { forecastGeometry, loadForecast, type ForecastPreview } from '../model/forecast-preview';
import { state } from '../store';

const props = defineProps<{ entity: string }>();
const forecast = ref<ForecastPreview | null>(null), loading = ref(true);
watch(() => [props.entity, Math.floor(state.now / 60000)], async (_, __, cleanup) => {
  let active = true; cleanup(() => { active = false; });
  loading.value = true; forecast.value = null;
  try { const value = await loadForecast(props.entity); if (active) forecast.value = value; }
  catch { /* A sensor without a current forecast stays explicitly empty. */ }
  finally { if (active) loading.value = false; }
}, { immediate: true });
const geometry = computed(() => forecast.value ? forecastGeometry(forecast.value) : null);
const caption = computed(() => forecast.value
  ? `${new Date(forecast.value.start * 1000).toLocaleString()} – ${new Date(forecast.value.end * 1000).toLocaleString()}`
  : '');
</script>
<template>
  <div class="price-forecast">
    <div class="forecast-heading"><span>{{ t('editor.pages.forecast_label') }}</span><small>{{ forecast?.unit }}</small></div>
    <svg v-if="geometry" viewBox="0 0 200 56" role="img" :aria-label="t('editor.pages.forecast_label')">
      <title>{{ caption }}</title>
      <g class="y-axis">
        <template v-for="(tick, index) in geometry.yTicks" :key="index">
          <text x="27" :y="tick.y + 2" text-anchor="end">{{ tick.label }}</text>
          <path d="M30 0 H197" :transform="`translate(0 ${tick.y})`" />
        </template>
      </g>
      <path v-for="(path, index) in geometry.paths" :key="index" class="step-curve" :d="path" />
      <g class="x-axis">
        <text v-for="(tick, index) in geometry.xTicks" :key="index" :x="tick.x" y="54" text-anchor="middle">{{ tick.label }}</text>
      </g>
    </svg>
    <small v-else class="forecast-empty">{{ t(loading ? 'editor.pages.forecast_loading' : 'editor.pages.forecast_empty') }}</small>
  </div>
</template>
<style scoped>
.price-forecast { width: 100%; min-width: 0; }
.forecast-heading { display: flex; justify-content: space-between; gap: 4px; font-size: 9px; color: #46525e; }
.forecast-heading small { overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
svg { display: block; width: 100%; min-height: 24px; max-height: 65px; overflow: visible; }
.step-curve { fill: none; stroke: var(--tile-accent); stroke-width: 2; stroke-linecap: square; stroke-linejoin: miter; }
.y-axis path { fill: none; stroke: #d6dce1; stroke-width: .5; }
.y-axis text, .x-axis text { fill: #64717c; font-size: 5px; }
.forecast-empty { font-size: 9px; color: #46525e; }
</style>
