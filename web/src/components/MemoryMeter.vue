<script setup lang="ts">
// How much of the screen's memory for tiles the layout takes (firmware 0.34.0+, model/memory.ts): a thin bar and a
// share beside the tile count, the kilobytes in its title. Quiet until it matters: the accent while there is room, amber
// from 80 %, red when it is full or over, which is also when the add-on refuses to save.
import { computed } from "vue";
import { t } from "../i18n";
import { kilobytes } from "../model/memory";
import { memory, screenMemory } from "../store";

const percent = computed(() => (memory.value ? Math.min(999, Math.round(memory.value.share * 100)) : 0));
const title = computed(() => {
  const use = memory.value, said = screenMemory.value;
  if (!use || !said) return "";
  const lines = [t("editor.memory.title", { need: kilobytes(use.need, true), room: kilobytes(use.room) })];
  if (use.level === "over") lines.push(t("editor.memory.over"));
  if (said.short) lines.push(t("editor.memory.short"));
  if (said.live === false) lines.push(t("editor.memory.last_known"));
  return lines.join("\n");
});
</script>

<template>
  <span v-if="memory" id="memory" class="memory-meter" :class="memory.level" :title="title" role="meter"
    :aria-label="t('editor.memory.label')" aria-valuemin="0" aria-valuemax="100" :aria-valuenow="percent" :aria-valuetext="title">
    <span class="memory-bar" aria-hidden="true"><i :style="{ width: Math.min(100, percent) + '%' }"></i></span>
    <span class="memory-text">{{ t("editor.memory.share", { n: percent }) }}</span>
    <span v-if="screenMemory?.short" class="memory-short" aria-hidden="true"></span>
  </span>
</template>
