/// <reference types="vite/client" />

declare global {
  interface Window {
    sendDebugMessageToSKSE?: (value: string) => void
    updateFocusLabel?: (value: string) => void
  }
}

export {}
