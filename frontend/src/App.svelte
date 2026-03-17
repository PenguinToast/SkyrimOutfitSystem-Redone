<script lang="ts">
  import { onMount } from 'svelte'

  type OutfitPreset = {
    name: string
    slot: string
    summary: string
  }

  const presets: OutfitPreset[] = [
    {
      name: 'Road Leathers',
      slot: 'Travel',
      summary: 'Lightweight gear for cities, roads, and vendor loops.',
    },
    {
      name: 'Dwemer Breaker',
      slot: 'Dungeon',
      summary: 'Heavy utility loadout tuned for trapped ruins and close fights.',
    },
    {
      name: 'Court Presence',
      slot: 'Social',
      summary: 'A cleaner outfit profile for dialogue scenes and roleplay.',
    },
  ]

  let focusStatus = 'Hidden. Press F3 to open the outfit system UI.'
  let debugMessage = 'UI booted from Vite + Svelte.'
  let sendState = 'Idle'
  let bridgeReady = false
  let offsetX = 0
  let offsetY = 0
  let dragOriginX = 0
  let dragOriginY = 0
  let startOffsetX = 0
  let startOffsetY = 0
  let dragging = false

  $: windowStyle = `transform: translate(calc(-50% + ${offsetX}px), calc(-50% + ${offsetY}px));`

  function sendDebugMessage() {
    const payload = debugMessage.trim()

    if (typeof window.sendDebugMessageToSKSE !== 'function') {
      sendState = 'SKSE bridge unavailable'
      return
    }

    window.sendDebugMessageToSKSE(payload)
    sendState = payload.length > 0 ? `Sent: ${payload}` : 'Sent an empty debug message'
  }

  function closeWindow() {
    if (typeof window.hideOutfitSystemUI !== 'function') {
      sendState = 'Close bridge unavailable'
      return
    }

    window.hideOutfitSystemUI('close')
    sendState = 'Window hidden'
  }

  function beginDrag(event: PointerEvent) {
    const target = event.target
    if (target instanceof HTMLElement && target.closest('button')) {
      return
    }

    dragging = true
    dragOriginX = event.clientX
    dragOriginY = event.clientY
    startOffsetX = offsetX
    startOffsetY = offsetY
  }

  function handlePointerMove(event: PointerEvent) {
    if (!dragging) {
      return
    }

    offsetX = startOffsetX + (event.clientX - dragOriginX)
    offsetY = startOffsetY + (event.clientY - dragOriginY)
  }

  function endDrag() {
    dragging = false
  }

  onMount(() => {
    bridgeReady = typeof window.sendDebugMessageToSKSE === 'function'
    window.updateFocusLabel = (value: string) => {
      focusStatus = value
      bridgeReady = true
    }

    window.addEventListener('pointermove', handlePointerMove)
    window.addEventListener('pointerup', endDrag)
    window.addEventListener('pointercancel', endDrag)

    return () => {
      delete window.updateFocusLabel
      window.removeEventListener('pointermove', handlePointerMove)
      window.removeEventListener('pointerup', endDrag)
      window.removeEventListener('pointercancel', endDrag)
    }
  })
</script>

<svelte:head>
  <meta name="color-scheme" content="light" />
</svelte:head>

<main class="shell">
  <section class="window" class:dragging={dragging} style={windowStyle}>
    <header
      class="titlebar"
      role="toolbar"
      tabindex="0"
      aria-label="Window controls"
      onpointerdown={beginDrag}
    >
      <div class="titlecopy">
        <p class="eyebrow">Prisma UI Control Surface</p>
        <h1>Skyrim Outfit System NG</h1>
      </div>
      <button class="close-button" type="button" onclick={closeWindow} aria-label="Close outfit system">
        ×
      </button>
    </header>

    <div class="window-body">
      <section class="hero">
        <div class="hero-copy">
          <p class="lede">
            A Vite, Svelte, and TypeScript frontend wired for PrismaUI. This starter is ready for
            outfit browsing, slot management, and SKSE-backed interactions.
          </p>
        </div>

        <div class="status-card">
          <div class="status-row">
            <span>Bridge</span>
            <strong class:ready={bridgeReady}>{bridgeReady ? 'Ready' : 'Waiting'}</strong>
          </div>
          <div class="status-row">
            <span>Focus</span>
            <strong>{focusStatus}</strong>
          </div>
          <div class="status-row">
            <span>Debug</span>
            <strong>{sendState}</strong>
          </div>
        </div>
      </section>

      <section class="grid">
        <article class="panel">
          <div class="panel-head">
            <p class="panel-kicker">Starter Presets</p>
            <h2>Outfit slots worth building first</h2>
          </div>

          <div class="preset-list">
            {#each presets as preset}
              <div class="preset-card">
                <div>
                  <p class="slot">{preset.slot}</p>
                  <h3>{preset.name}</h3>
                </div>
                <p>{preset.summary}</p>
              </div>
            {/each}
          </div>
        </article>

        <article class="panel">
          <div class="panel-head">
            <p class="panel-kicker">SKSE Debug Link</p>
            <h2>Send a message into the plugin</h2>
          </div>

          <label class="field">
            <span>Payload</span>
            <textarea
              bind:value={debugMessage}
              rows="4"
              placeholder="Type a debug message for sendDebugMessageToSKSE"
            ></textarea>
          </label>

          <button class="action" type="button" onclick={sendDebugMessage}>
            Send to SKSE
          </button>
        </article>
      </section>
    </div>
  </section>
</main>
