/// <reference types="vite/client" />

declare global {
  interface Window {
    hideOutfitSystemUI?: (value: string) => void
    setEquipmentCatalog?: (payload: string | object) => void
    sendDebugMessageToSKSE?: (value: string) => void
    updateFocusLabel?: (value: string) => void
  }
}

export {}
