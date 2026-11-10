<script setup lang="ts">
import SerialEmulatorPanel from './components/SerialEmulatorPanel.vue'
</script>

<template>
  <main class="min-h-screen p-4">
    <div class="mx-auto max-w-3xl">
      <h1 class="mb-4 text-xl font-bold">DS5 Dongle Debugger</h1>
      <SerialEmulatorPanel />
    </div>
  </main>
</template>
