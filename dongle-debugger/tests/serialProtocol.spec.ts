import { describe, expect, it } from 'vitest'
import {
  DEBUG_FRAME_OVERHEAD,
  DebugFrameDecoder,
  DebugMessageType,
  decodeDebugFrame,
  encodeDebugFrame,
} from '@/utils/serial/serialProtocol'

describe('DS5 debug serial protocol', () => {
  it('round-trips a frame', () => {
    const frame = encodeDebugFrame({
      type: DebugMessageType.InputReport,
      sequence: 7,
      payload: new Uint8Array([1, 2, 3, 4]),
    })
    expect(frame.byteLength).toBe(DEBUG_FRAME_OVERHEAD + 4)
    expect(decodeDebugFrame(frame)).toMatchObject({
      type: DebugMessageType.InputReport,
      sequence: 7,
    })
    expect(Array.from(decodeDebugFrame(frame)!.payload)).toEqual([1, 2, 3, 4])
  })

  it('rejects corrupted frames', () => {
    const frame = encodeDebugFrame({ type: DebugMessageType.Ping, sequence: 1, payload: new Uint8Array([9]) })
    frame[frame.length - 1] ^= 0xFF
    expect(decodeDebugFrame(frame)).toBeUndefined()
  })

  it('decodes split and concatenated frames', () => {
    const first = encodeDebugFrame({ type: DebugMessageType.Hello, sequence: 1, payload: new Uint8Array() })
    const second = encodeDebugFrame({ type: DebugMessageType.Pong, sequence: 2, payload: new Uint8Array([8]) })
    const decoder = new DebugFrameDecoder()
    expect(decoder.push(first.slice(0, 3))).toHaveLength(0)
    expect(decoder.push(new Uint8Array([...first.slice(3), ...second]))).toHaveLength(2)
  })

  it('resynchronizes after noise', () => {
    const frame = encodeDebugFrame({ type: DebugMessageType.Claim, sequence: 3, payload: new Uint8Array() })
    const decoder = new DebugFrameDecoder()
    expect(decoder.push(new Uint8Array([0, 1, 2, ...frame]))[0].type).toBe(DebugMessageType.Claim)
  })
})
