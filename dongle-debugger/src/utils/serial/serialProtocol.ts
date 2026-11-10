export const DEBUG_SYNC = 0xD5
export const DEBUG_VERSION = 1
export const DEBUG_MAX_PAYLOAD = 80
export const DEBUG_FRAME_OVERHEAD = 9

export const enum DebugMessageType {
  Hello = 0x01,
  HelloAck = 0x02,
  Claim = 0x03,
  Release = 0x04,
  InputReport = 0x10,
  OutputReport = 0x11,
  FeatureRequest = 0x12,
  FeatureResponse = 0x13,
  Status = 0x20,
  Ping = 0x30,
  Pong = 0x31,
  Scan = 0x40,
  Connect = 0x41,
  Disconnect = 0x42,
  ScanResult = 0x43,
  ScanEnd = 0x44,
}

export interface DebugFrame {
  type: number
  flags?: number
  sequence: number
  payload: Uint8Array
}

function crc16(data: Uint8Array): number {
  let crc = 0xFFFF
  for (const byte of data) {
    crc ^= byte
    for (let bit = 0; bit < 8; bit++) {
      crc = (crc & 1) ? (crc >>> 1) ^ 0xA001 : crc >>> 1
    }
  }
  return crc
}

export function encodeDebugFrame(frame: DebugFrame): Uint8Array {
  if (frame.payload.byteLength > DEBUG_MAX_PAYLOAD) {
    throw new RangeError('Debug payload is too large')
  }
  const result = new Uint8Array(DEBUG_FRAME_OVERHEAD + frame.payload.byteLength)
  result[0] = DEBUG_SYNC
  result[1] = DEBUG_VERSION
  result[2] = frame.type
  result[3] = frame.flags ?? 0
  result[4] = frame.payload.byteLength
  result[5] = frame.payload.byteLength >>> 8
  result[6] = frame.sequence & 0xFF
  result.set(frame.payload, 7)
  const crc = crc16(result.subarray(1, 7 + frame.payload.byteLength))
  result[7 + frame.payload.byteLength] = crc & 0xFF
  result[8 + frame.payload.byteLength] = crc >>> 8
  return result
}

export function decodeDebugFrame(data: Uint8Array): DebugFrame | undefined {
  if (data.byteLength < DEBUG_FRAME_OVERHEAD || data[0] !== DEBUG_SYNC || data[1] !== DEBUG_VERSION) {
    return undefined
  }
  const length = data[4] | (data[5] << 8)
  const total = DEBUG_FRAME_OVERHEAD + length
  if (length > DEBUG_MAX_PAYLOAD || data.byteLength < total) {
    return undefined
  }
  const expected = data[7 + length] | (data[8 + length] << 8)
  const actual = crc16(data.subarray(1, 7 + length))
  if (actual !== expected) {
    return undefined
  }
  return {
    type: data[2],
    flags: data[3],
    sequence: data[6],
    payload: data.slice(7, 7 + length),
  }
}

export class DebugFrameDecoder {
  private buffer = new Uint8Array()

  push(chunk: Uint8Array): DebugFrame[] {
    const combined = new Uint8Array(this.buffer.byteLength + chunk.byteLength)
    combined.set(this.buffer)
    combined.set(chunk, this.buffer.byteLength)
    this.buffer = combined
    const frames: DebugFrame[] = []
    while (this.buffer.byteLength >= DEBUG_FRAME_OVERHEAD) {
      const sync = this.buffer.indexOf(DEBUG_SYNC)
      if (sync < 0) {
        this.buffer = new Uint8Array()
        break
      }
      if (sync > 0) {
        this.buffer = this.buffer.slice(sync)
      }
      const length = this.buffer[4] | (this.buffer[5] << 8)
      const total = DEBUG_FRAME_OVERHEAD + length
      if (length > DEBUG_MAX_PAYLOAD) {
        this.buffer = this.buffer.slice(1)
        continue
      }
      if (this.buffer.byteLength < total) {
        break
      }
      const frame = decodeDebugFrame(this.buffer.slice(0, total))
      this.buffer = this.buffer.slice(total)
      if (frame) {
        frames.push(frame)
      }
    }
    return frames
  }
}
