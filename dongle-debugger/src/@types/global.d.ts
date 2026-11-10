declare global {
  interface SerialPort {
    readable: ReadableStream<Uint8Array> | null
    writable: WritableStream<Uint8Array> | null
    open(options: { baudRate: number }): Promise<void>
    close(): Promise<void>
  }

  interface Serial {
    requestPort(): Promise<SerialPort>
  }

  interface Navigator {
    serial: Serial
  }

  interface Document {
    startViewTransition?: (callback: () => Promise<void> | void) => {
      finished: Promise<void>
      updateCallbackDone: Promise<void>
      ready: Promise<void>
    }
  }

  interface Window {
    debug: any
  }
}

export {}
