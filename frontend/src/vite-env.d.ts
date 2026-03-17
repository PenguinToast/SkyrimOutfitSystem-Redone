/// <reference types="vite/client" />

declare global {
  interface Window {
    hideOutfitSystemUI?: (value: string) => void
    sendDebugMessageToSKSE?: (value: string) => void
    updateFocusLabel?: (value: string) => void
  }
}

export {}
