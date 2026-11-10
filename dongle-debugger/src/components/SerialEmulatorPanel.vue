<script setup lang="ts">
import { onBeforeUnmount, onMounted, ref } from 'vue'
import type { ScanResult } from '@/utils/serial/WebSerialDualSenseTransport'
import DouButton from '@/components/base/DouButton.vue'
import { WebSerialDualSenseTransport } from '@/utils/serial/WebSerialDualSenseTransport'

const transport = new WebSerialDualSenseTransport()
const connected = ref(false)
const claimed = ref(false)
const error = ref('')
const scanResults = ref<ScanResult[]>([])
const scanning = ref(false)
const rawLog = ref<Array<{ direction: 'tx' | 'rx', timestamp: string, payload: string }>>([])
let timer: number | undefined
const removeScanHandler = transport.onScanResult(result => {
  scanResults.value.push(result)
})
const removeScanStateHandler = transport.onScanState(value => {
  scanning.value = value
})

function timestamp() {
  const now = new Date()
  const pad = (value: number) => value.toString().padStart(2, '0')
  return `${now.getFullYear().toString().slice(-2)}-${pad(now.getMonth() + 1)}-${pad(now.getDate())}|${pad(now.getHours())}:${pad(now.getMinutes())}:${pad(now.getSeconds())}`
}

const removeRawDataHandler = transport.onRawData((direction, data) => {
  const payload = [...data].map(byte => byte.toString(16).padStart(2, '0')).join(' ')
  rawLog.value.push({ direction, timestamp: timestamp(), payload })
  if (rawLog.value.length > 200) {
    rawLog.value.splice(0, rawLog.value.length - 200)
  }
})

const report = new Uint8Array(63)

function addressText(address: Uint8Array) {
  return [...address].map(byte => byte.toString(16).padStart(2, '0')).join(':')
}

async function scan() {
  try {
    error.value = ''
    scanResults.value = []
    await transport.scan()
  }
  catch (value) {
    error.value = value instanceof Error ? value.message : String(value)
  }
}

async function connectBluetooth(result: ScanResult) {
  try {
    error.value = ''
    await transport.connectBluetooth(result.address)
  }
  catch (value) {
    error.value = value instanceof Error ? value.message : String(value)
  }
}

function neutralReport() {
  report.fill(0)
  report[0] = 0x80
  report[1] = 0x80
  report[2] = 0x80
  report[3] = 0x80
  report[8] = 0x08
}

async function connect() {
  try {
    error.value = ''
    await transport.open()
    connected.value = true
  }
  catch (value) {
    connected.value = false
    claimed.value = false
    stopReports()
    error.value = value instanceof Error ? value.message : String(value)
  }
}

async function toggleClaim() {
  try {
    error.value = ''
    if (claimed.value) {
      await transport.release()
      claimed.value = false
      stopReports()
    }
    else {
      await transport.claim()
      claimed.value = true
      neutralReport()
      timer = window.setInterval(() => {
        void transport.sendInputReport(0x01, report)
      }, 16)
    }
  }
  catch (value) {
    error.value = value instanceof Error ? value.message : String(value)
  }
}

function setButton(index: number, down: boolean) {
  if (down) {
    report[4] |= 1 << index
  }
  else {
    report[4] &= ~(1 << index)
  }
}

function onKey(event: KeyboardEvent, down: boolean) {
  const key = event.key.toLowerCase()
  const button = { square: 0, cross: 1, circle: 2, triangle: 3 }[key]
  if (button !== undefined) {
    setButton(button, down)
    event.preventDefault()
  }
  const axes: Record<string, [number, number]> = {
    arrowup: [0, 0x00], arrowdown: [0, 0xFF], arrowleft: [1, 0x00], arrowright: [1, 0xFF],
  }
  const axis = axes[key]
  if (axis) {
    report[axis[0]] = axis[1]
    event.preventDefault()
  }
}

function handleKeyDown(event: KeyboardEvent) {
  onKey(event, true)
}

function handleKeyUp(event: KeyboardEvent) {
  onKey(event, false)
}

onMounted(() => {
  window.addEventListener('keydown', handleKeyDown)
  window.addEventListener('keyup', handleKeyUp)
})

onBeforeUnmount(() => {
  removeScanHandler()
  removeScanStateHandler()
  removeRawDataHandler()
  window.removeEventListener('keydown', handleKeyDown)
  window.removeEventListener('keyup', handleKeyUp)
  void disconnect()
})

function sendNeutral() {
  neutralReport()
  void transport.sendInputReport(0x01, report).catch(value => {
    error.value = value instanceof Error ? value.message : String(value)
  })
}

function stopReports() {
  if (timer !== undefined) {
    window.clearInterval(timer)
    timer = undefined
  }
}

async function disconnect() {
  stopReports()
  await transport.close()
  connected.value = false
  claimed.value = false
}

</script>

<template>
  <div class="flex flex-wrap items-center gap-2 rounded-xl border border-primary/30 p-2">
    <span class="font-bold">DS5Dongle WebSerial</span>
    <DouButton v-if="!connected" @click="connect">连接 ESP32</DouButton>
    <template v-else>
      <DouButton @click="scan" :disabled="scanning">{{ scanning ? '扫描中…' : '扫描蓝牙' }}</DouButton>
      <DouButton @click="toggleClaim">{{ claimed ? '释放输入接管' : '激活模拟输入' }}</DouButton>
      <DouButton v-if="claimed" @click="sendNeutral">发送中立报告</DouButton>
      <DouButton @click="disconnect">断开</DouButton>
      <span class="text-sm opacity-70">{{ claimed ? '已接管' : '已连接' }}</span>
      <div class="mt-2 w-full">
        <div class="mb-1 flex items-center gap-2 text-sm font-bold">
          <span>原始串口数据</span>
          <DouButton @click="rawLog = []">清空</DouButton>
        </div>
        <div class="max-h-48 overflow-auto rounded bg-black p-2 font-mono text-xs">
          <div v-for="(entry, index) in rawLog" :key="index" :class="entry.direction === 'rx' ? 'text-blue-400' : 'text-green-400'" class="mb-2 last:mb-0">
            <div>[{{ entry.timestamp }}]{{ entry.direction.toUpperCase() }}:</div>
            <div class="break-all whitespace-pre-wrap">{{ entry.payload }}</div>
          </div>
          <div v-if="!rawLog.length" class="text-gray-400">等待串口数据…</div>
        </div>
      </div>
      <div v-if="scanResults.length" class="w-full space-y-1 text-sm">
        <div v-for="result in scanResults" :key="addressText(result.address)" class="flex items-center gap-2">
          <span class="font-mono">{{ addressText(result.address) }}</span>
          <span>{{ result.name || 'Unknown' }}</span>
          <span class="opacity-70">{{ result.rssi }} dBm</span>
          <DouButton @click="connectBluetooth(result)">连接</DouButton>
        </div>
      </div>
    </template>
    <span v-if="error" class="w-full text-sm text-red-500">{{ error }}</span>
  </div>
</template>
