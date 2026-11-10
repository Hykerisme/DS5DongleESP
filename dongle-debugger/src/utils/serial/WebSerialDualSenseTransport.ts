import {
  DebugFrameDecoder,
  DebugMessageType,
  encodeDebugFrame,
} from './serialProtocol'

export interface SerialStatus {
  claimed: boolean
  connected: boolean
  lastStatus?: number
}

export interface ScanResult {
  address: Uint8Array
  rssi: number
  transport: number
  name: string
}

type InputHandler = (reportId: number, data: DataView) => void
type OutputHandler = (reportId: number, data: DataView) => void
type ScanHandler = (result: ScanResult) => void
type ScanStateHandler = (scanning: boolean) => void
type StatusHandler = (status: SerialStatus) => void
type RawDataHandler = (direction: 'tx' | 'rx', data: Uint8Array) => void
type FeatureRequest = {
  resolve: (data: DataView) => void
  reject: (error: Error) => void
  timeout: ReturnType<typeof setTimeout>
}

export class WebSerialDualSenseTransport {
  private port?: SerialPort
  private reader?: ReadableStreamDefaultReader<Uint8Array>
  private sequence = 0
  private decoder = new DebugFrameDecoder()
  private writeChain: Promise<void> = Promise.resolve()
  private readTask?: Promise<void>
  private claimed = false
  private connected = false
  private inputHandlers = new Set<InputHandler>()
  private outputHandlers = new Set<OutputHandler>()
  private scanHandlers = new Set<ScanHandler>()
  private scanStateHandlers = new Set<ScanStateHandler>()
  private statusHandlers = new Set<StatusHandler>()
  private rawDataHandlers = new Set<RawDataHandler>()
  private pendingFeatures = new Map<number, FeatureRequest>()
  private nextRequestId = 0
  private helloResolve?: () => void
  private helloReject?: (error: Error) => void

  async open(): Promise<void> {
    if (this.port) {
      return
    }
    this.port = await navigator.serial.requestPort()
    await this.port.open({ baudRate: 2_000_000 })
    this.connected = true
    this.emitStatus()
    this.readTask = this.readLoop()
    try {
      await new Promise<void>((resolve, reject) => {
        this.helloResolve = resolve
        this.helloReject = reject
        void this.sendFrame(DebugMessageType.Hello)
        setTimeout(() => reject(new Error('ESP32 debug handshake timed out')), 1500)
      })
    }
    catch (error) {
      await this.close()
      throw error
    }
  }

  async close(): Promise<void> {
    this.rejectPendingFeatures(new Error('Serial port closed'))
    this.claimed = false
    this.connected = false
    try {
      if (this.port?.writable) {
        await this.sendFrame(DebugMessageType.Release)
      }
    }
    catch {
      // The serial device may already be disconnected.
    }
    await this.reader?.cancel().catch(() => undefined)
    await this.readTask?.catch(() => undefined)
    await this.port?.close().catch(() => undefined)
    this.reader = undefined
    this.port = undefined
    this.readTask = undefined
    this.emitStatus()
  }

  async claim(): Promise<void> {
    this.ensureOpen()
    await this.sendFrame(DebugMessageType.Claim)
    this.claimed = true
    this.emitStatus()
  }

  async release(): Promise<void> {
    this.ensureOpen()
    await this.sendFrame(DebugMessageType.Release)
    this.claimed = false
    this.emitStatus()
  }

  async scan(): Promise<void> {
    this.ensureOpen()
    for (const handler of this.scanStateHandlers) {
      handler(true)
    }
    await this.sendFrame(DebugMessageType.Scan)
  }

  async connectBluetooth(address: Uint8Array): Promise<void> {
    this.ensureOpen()
    if (address.byteLength !== 6) {
      throw new RangeError('Bluetooth address must contain 6 bytes')
    }
    await this.sendFrame(DebugMessageType.Connect, address)
  }

  async disconnectBluetooth(): Promise<void> {
    this.ensureOpen()
    await this.sendFrame(DebugMessageType.Disconnect)
  }
  async sendInputReport(reportId: number, data: Uint8Array): Promise<void> {
    this.ensureOpen()
    if (!this.claimed) {
      throw new Error('WebSerial emulator is not claimed')
    }
    if (data.byteLength > 78) {
      throw new RangeError('DualSense input report is too large for debug protocol')
    }
    const payload = new Uint8Array(data.byteLength + 2)
    payload[0] = reportId & 0xFF
    payload[1] = reportId >>> 8
    payload.set(data, 2)
    await this.sendFrame(DebugMessageType.InputReport, payload)
  }

  async sendOutputReport(reportId: number, data: Uint8Array): Promise<void> {
    this.ensureOpen()
    const payload = new Uint8Array(data.byteLength + 2)
    payload[0] = reportId & 0xFF
    payload[1] = reportId >>> 8
    payload.set(data, 2)
    await this.sendFrame(DebugMessageType.OutputReport, payload)
  }

  async requestFeatureReport(reportId: number): Promise<DataView> {
    this.ensureOpen()
    const requestId = this.nextRequestId++ & 0xFF
    const payload = new Uint8Array([reportId & 0xFF, reportId >>> 8, requestId])
    const response = new Promise<DataView>((resolve, reject) => {
      const timeout = setTimeout(() => {
        this.pendingFeatures.delete(requestId)
        reject(new Error(`Feature report request ${requestId} timed out`))
      }, 1500)
      this.pendingFeatures.set(requestId, { resolve, reject, timeout })
    })
    try {
      await this.sendFrame(DebugMessageType.FeatureRequest, payload)
    }
    catch (error) {
      const pending = this.pendingFeatures.get(requestId)
      if (pending) {
        clearTimeout(pending.timeout)
        this.pendingFeatures.delete(requestId)
        pending.reject(error instanceof Error ? error : new Error(String(error)))
      }
    }
    return response
  }

  onInputReport(handler: InputHandler): () => void {
    this.inputHandlers.add(handler)
    return () => this.inputHandlers.delete(handler)
  }

  onOutputReport(handler: OutputHandler): () => void {
    this.outputHandlers.add(handler)
    return () => this.outputHandlers.delete(handler)
  }

  onScanResult(handler: ScanHandler): () => void {
    this.scanHandlers.add(handler)
    return () => this.scanHandlers.delete(handler)
  }

  onScanState(handler: ScanStateHandler): () => void {
    this.scanStateHandlers.add(handler)
    return () => this.scanStateHandlers.delete(handler)
  }

  onStatus(handler: StatusHandler): () => void {
    this.statusHandlers.add(handler)
    return () => this.statusHandlers.delete(handler)
  }

  onRawData(handler: RawDataHandler): () => void {
    this.rawDataHandlers.add(handler)
    return () => this.rawDataHandlers.delete(handler)
  }

  getStatus(): SerialStatus {
    return { claimed: this.claimed, connected: this.connected }
  }

  private async sendFrame(type: DebugMessageType, payload = new Uint8Array()): Promise<void> {
    if (!this.port?.writable) {
      throw new Error('Serial port is not open')
    }
    const frame = encodeDebugFrame({ type, sequence: this.sequence++, payload })
    this.writeChain = this.writeChain.then(async () => {
      const writer = this.port!.writable!.getWriter()
      try {
        await writer.write(frame)
        for (const handler of this.rawDataHandlers) {
          handler('tx', frame.slice())
        }
      }
      finally {
        writer.releaseLock()
      }
    })
    return this.writeChain
  }

  private async readLoop(): Promise<void> {
    try {
      while (this.port?.readable) {
        this.reader = this.port.readable.getReader()
        try {
          while (true) {
            const result = await this.reader.read()
            if (result.done) {
              break
            }
            for (const handler of this.rawDataHandlers) {
              handler('rx', result.value.slice())
            }
            for (const frame of this.decoder.push(result.value)) {
              this.handleFrame(frame)
            }
          }
        }
        finally {
          this.reader.releaseLock()
          this.reader = undefined
        }
        break
      }
    }
    catch (error) {
      this.connected = false
      this.rejectPendingFeatures(error instanceof Error ? error : new Error(String(error)))
      this.helloReject?.(error instanceof Error ? error : new Error(String(error)))
      this.emitStatus()
    }
  }

  private handleFrame(frame: ReturnType<DebugFrameDecoder['push']>[number]): void {
    if (frame.type === DebugMessageType.HelloAck) {
      this.helloResolve?.()
      this.helloResolve = undefined
      this.helloReject = undefined
      return
    }
    if (frame.type === DebugMessageType.Status) {
      this.emitStatus(frame.payload[0])
    }
    if (frame.type === DebugMessageType.ScanResult && frame.payload.byteLength >= 9) {
      const nameLength = Math.min(frame.payload[8], frame.payload.byteLength - 9)
      const nameBytes = frame.payload.slice(9, 9 + nameLength)
      const name = new TextDecoder().decode(nameBytes)
      const result: ScanResult = {
        address: frame.payload.slice(0, 6),
        rssi: frame.payload[6] > 127 ? frame.payload[6] - 256 : frame.payload[6],
        transport: frame.payload[7],
        name,
      }
      for (const handler of this.scanHandlers) {
        handler(result)
      }
      return
    }
    if (frame.type === DebugMessageType.ScanEnd) {
      for (const handler of this.scanStateHandlers) {
        handler(false)
      }
      return
    }
    if (frame.type === DebugMessageType.FeatureResponse && frame.payload.byteLength >= 3) {
      const requestId = frame.payload[2]
      const pending = this.pendingFeatures.get(requestId)
      if (pending) {
        clearTimeout(pending.timeout)
        this.pendingFeatures.delete(requestId)
        pending.resolve(new DataView(frame.payload.buffer.slice(frame.payload.byteOffset + 3, frame.payload.byteOffset + frame.payload.byteLength)))
      }
      return
    }
    if (frame.type === DebugMessageType.OutputReport && frame.payload.byteLength >= 2) {
      const reportId = frame.payload[0] | (frame.payload[1] << 8)
      const data = new DataView(frame.payload.buffer.slice(frame.payload.byteOffset + 2, frame.payload.byteOffset + frame.payload.byteLength))
      for (const handler of this.outputHandlers) {
        handler(reportId, data)
      }
    }
  }

  private rejectPendingFeatures(error: Error): void {
    for (const [requestId, pending] of this.pendingFeatures) {
      clearTimeout(pending.timeout)
      pending.reject(error)
      this.pendingFeatures.delete(requestId)
    }
  }

  private emitStatus(lastStatus?: number): void {
    const status = { claimed: this.claimed, connected: this.connected, lastStatus }
    for (const handler of this.statusHandlers) {
      handler(status)
    }
  }

  private ensureOpen(): void {
    if (!this.port || !this.connected) {
      throw new Error('Serial port is not open')
    }
  }
}
