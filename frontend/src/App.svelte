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

  let focusStatus = 'Waiting for SKSE to initialize PrismaUI.'
  let debugMessage = 'UI booted from Vite + Svelte.'
  let sendState = 'Idle'
  let bridgeReady = false

  function sendDebugMessage() {
    const payload = debugMessage.trim()

    if (typeof window.sendDebugMessageToSKSE !== 'function') {
      sendState = 'SKSE bridge unavailable'
      return
    }

    window.sendDebugMessageToSKSE(payload)
    sendState = payload.length > 0 ? `Sent: ${payload}` : 'Sent an empty debug message'
  }

  onMount(() => {
    bridgeReady = typeof window.sendDebugMessageToSKSE === 'function'
    window.updateFocusLabel = (value: string) => {
      focusStatus = value
      bridgeReady = true
    }

    return () => {
      delete window.updateFocusLabel
    }
  })
</script>

<svelte:head>
  <meta name="color-scheme" content="light" />
</svelte:head>

<main class="shell">
  <section class="hero">
    <div class="hero-copy">
      <p class="eyebrow">Prisma UI Control Surface</p>
      <h1>Skyrim Outfit System NG</h1>
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
</main>
