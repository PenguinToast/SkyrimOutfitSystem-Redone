<script lang="ts">
  import { onMount } from 'svelte'
  import {
    defaultCatalog,
    type EquipmentCatalog,
    type GearEntry,
    type OutfitEntry,
  } from './lib/catalog'

  type BrowserTab = 'gear' | 'outfits'
  type GearSort = 'name' | 'plugin' | 'slot' | 'weight' | 'value' | 'stat'
  type OutfitSort = 'name' | 'plugin' | 'pieces'
  type SortDirection = 'asc' | 'desc'

  let catalog: EquipmentCatalog = defaultCatalog
  let focusStatus = 'Hidden. Press F3 to open the outfit system UI.'
  let sendState = 'Idle'
  let bridgeReady = false
  let activeTab: BrowserTab = 'gear'

  let gearSearch = ''
  let gearKind: 'all' | 'armor' | 'weapon' = 'all'
  let gearPlugin = 'All plugins'
  let gearSlot = 'Any slot'
  let gearSort: GearSort = 'name'
  let gearSortDirection: SortDirection = 'asc'

  let outfitSearch = ''
  let outfitPlugin = 'All plugins'
  let outfitTag = 'Any tag'
  let outfitSort: OutfitSort = 'name'
  let outfitSortDirection: SortDirection = 'asc'

  let offsetX = 0
  let offsetY = 0
  let dragOriginX = 0
  let dragOriginY = 0
  let startOffsetX = 0
  let startOffsetY = 0
  let dragging = false
  let dragFrame = 0
  let queuedPointerX = 0
  let queuedPointerY = 0

  $: gearPlugins = ['All plugins', ...new Set(catalog.gear.map((entry) => entry.plugin).sort())]
  $: gearSlots = [
    'Any slot',
    ...new Set(
      catalog.gear
        .filter((entry) => gearKind === 'all' || entry.kind === gearKind)
        .map((entry) => entry.slot)
        .sort(),
    ),
  ]

  $: outfitPlugins = ['All plugins', ...new Set(catalog.outfits.map((entry) => entry.plugin).sort())]
  $: outfitTags = [
    'Any tag',
    ...new Set(
      catalog.outfits
        .flatMap((entry) => entry.tags)
        .sort((left, right) => left.localeCompare(right)),
    ),
  ]

  $: filteredGear = sortGear(
    catalog.gear.filter((entry) => matchesGear(entry)),
    gearSort,
    gearSortDirection,
  )

  $: filteredOutfits = sortOutfits(
    catalog.outfits.filter((entry) => matchesOutfit(entry)),
    outfitSort,
    outfitSortDirection,
  )

  $: visibleGearCount = filteredGear.length
  $: visibleOutfitCount = filteredOutfits.length
  $: windowStyle = `transform: translate3d(calc(-50% + ${offsetX}px), calc(-50% + ${offsetY}px), 0);`

  function normalize(value: string) {
    return value.toLowerCase().trim()
  }

  function compareStrings(left: string, right: string, direction: SortDirection) {
    const result = left.localeCompare(right, undefined, { sensitivity: 'base' })
    return direction === 'asc' ? result : -result
  }

  function compareNumbers(left: number, right: number, direction: SortDirection) {
    const result = left - right
    return direction === 'asc' ? result : -result
  }

  function gearStat(entry: GearEntry) {
    return entry.kind === 'armor' ? entry.armorRating ?? 0 : entry.damage ?? 0
  }

  function matchesGear(entry: GearEntry) {
    const query = normalize(gearSearch)
    const haystack = normalize(
      [
        entry.name,
        entry.editorId,
        entry.plugin,
        entry.category,
        entry.slot,
        ...entry.keywords,
      ].join(' '),
    )

    const queryMatch = query.length === 0 || haystack.includes(query)
    const kindMatch = gearKind === 'all' || entry.kind === gearKind
    const pluginMatch = gearPlugin === 'All plugins' || entry.plugin === gearPlugin
    const slotMatch = gearSlot === 'Any slot' || entry.slot === gearSlot

    return queryMatch && kindMatch && pluginMatch && slotMatch
  }

  function matchesOutfit(entry: OutfitEntry) {
    const query = normalize(outfitSearch)
    const haystack = normalize(
      [
        entry.name,
        entry.editorId,
        entry.plugin,
        entry.summary,
        ...entry.tags,
        ...entry.pieces,
      ].join(' '),
    )

    const queryMatch = query.length === 0 || haystack.includes(query)
    const pluginMatch = outfitPlugin === 'All plugins' || entry.plugin === outfitPlugin
    const tagMatch = outfitTag === 'Any tag' || entry.tags.includes(outfitTag)

    return queryMatch && pluginMatch && tagMatch
  }

  function sortGear(entries: GearEntry[], sortBy: GearSort, direction: SortDirection) {
    return [...entries].sort((left, right) => {
      switch (sortBy) {
        case 'plugin':
          return compareStrings(left.plugin, right.plugin, direction)
        case 'slot':
          return compareStrings(left.slot, right.slot, direction)
        case 'weight':
          return compareNumbers(left.weight, right.weight, direction)
        case 'value':
          return compareNumbers(left.value, right.value, direction)
        case 'stat':
          return compareNumbers(gearStat(left), gearStat(right), direction)
        case 'name':
        default:
          return compareStrings(left.name, right.name, direction)
      }
    })
  }

  function sortOutfits(entries: OutfitEntry[], sortBy: OutfitSort, direction: SortDirection) {
    return [...entries].sort((left, right) => {
      switch (sortBy) {
        case 'plugin':
          return compareStrings(left.plugin, right.plugin, direction)
        case 'pieces':
          return compareNumbers(left.pieces.length, right.pieces.length, direction)
        case 'name':
        default:
          return compareStrings(left.name, right.name, direction)
      }
    })
  }

  function toggleGearSort(nextSort: GearSort) {
    if (gearSort === nextSort) {
      gearSortDirection = gearSortDirection === 'asc' ? 'desc' : 'asc'
      return
    }

    gearSort = nextSort
    gearSortDirection = nextSort === 'stat' || nextSort === 'value' ? 'desc' : 'asc'
  }

  function toggleOutfitSort(nextSort: OutfitSort) {
    if (outfitSort === nextSort) {
      outfitSortDirection = outfitSortDirection === 'asc' ? 'desc' : 'asc'
      return
    }

    outfitSort = nextSort
    outfitSortDirection = nextSort === 'pieces' ? 'desc' : 'asc'
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
    queuedPointerX = event.clientX
    queuedPointerY = event.clientY
    startOffsetX = offsetX
    startOffsetY = offsetY
  }

  function flushDrag() {
    dragFrame = 0
    offsetX = startOffsetX + (queuedPointerX - dragOriginX)
    offsetY = startOffsetY + (queuedPointerY - dragOriginY)
  }

  function handlePointerMove(event: PointerEvent) {
    if (!dragging) {
      return
    }

    queuedPointerX = event.clientX
    queuedPointerY = event.clientY

    if (dragFrame !== 0) {
      return
    }

    dragFrame = window.requestAnimationFrame(flushDrag)
  }

  function endDrag() {
    if (dragFrame !== 0) {
      window.cancelAnimationFrame(dragFrame)
      flushDrag()
    }

    dragging = false
  }

  function setEquipmentCatalog(payload: string | object) {
    try {
      catalog =
        typeof payload === 'string'
          ? (JSON.parse(payload) as EquipmentCatalog)
          : (payload as EquipmentCatalog)
      sendState = `Catalog refreshed from ${catalog.source}`
    } catch {
      sendState = 'Catalog refresh failed'
    }
  }

  onMount(() => {
    bridgeReady = typeof window.sendDebugMessageToSKSE === 'function'
    window.updateFocusLabel = (value: string) => {
      focusStatus = value
      bridgeReady = true
    }
    window.setEquipmentCatalog = setEquipmentCatalog

    window.addEventListener('pointermove', handlePointerMove)
    window.addEventListener('pointerup', endDrag)
    window.addEventListener('pointercancel', endDrag)

    return () => {
      if (dragFrame !== 0) {
        window.cancelAnimationFrame(dragFrame)
      }

      delete window.updateFocusLabel
      delete window.setEquipmentCatalog
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
        <p class="eyebrow">Vanity Outfit Browser</p>
        <h1>Skyrim Outfit System NG</h1>
      </div>
      <button class="close-button" type="button" onclick={closeWindow} aria-label="Close outfit system">
        ×
      </button>
    </header>

    <div class="window-body">
      <section class="overview">
        <article class="hero-card">
          <p class="panel-kicker">Current Milestone</p>
          <h2>Browse every installed armor, weapon, and outfit</h2>
          <p class="lede">
            This selector is focused on the catalog side of the mod: search, sort, filter, and
            inspect installed gear now; wire the visual-swapping layer through a DAV fork later.
          </p>
        </article>

        <article class="status-card">
          <div class="status-row">
            <span>Catalog source</span>
            <strong>{catalog.source}</strong>
          </div>
          <div class="status-row">
            <span>Catalog revision</span>
            <strong>{catalog.revision}</strong>
          </div>
          <div class="status-row">
            <span>Focus</span>
            <strong>{focusStatus}</strong>
          </div>
          <div class="status-row">
            <span>Bridge</span>
            <strong class:ready={bridgeReady}>{bridgeReady ? 'Ready' : 'Waiting'}</strong>
          </div>
          <div class="status-row">
            <span>State</span>
            <strong>{sendState}</strong>
          </div>
        </article>
      </section>

      <nav class="tabs" aria-label="Equipment browser tabs">
        <button
          class:active={activeTab === 'gear'}
          type="button"
          onclick={() => (activeTab = 'gear')}
        >
          Gear
          <span>{catalog.gear.length}</span>
        </button>
        <button
          class:active={activeTab === 'outfits'}
          type="button"
          onclick={() => (activeTab = 'outfits')}
        >
          Outfits
          <span>{catalog.outfits.length}</span>
        </button>
      </nav>

      {#if activeTab === 'gear'}
        <section class="browser-panel">
          <div class="toolbar-grid">
            <label class="search-field search-wide">
              <span>Search installed gear</span>
              <input
                bind:value={gearSearch}
                type="search"
                placeholder="Name, editor ID, plugin, slot, keyword"
              />
            </label>

            <label class="search-field">
              <span>Plugin</span>
              <select bind:value={gearPlugin}>
                {#each gearPlugins as plugin}
                  <option value={plugin}>{plugin}</option>
                {/each}
              </select>
            </label>

            <label class="search-field">
              <span>Slot / class</span>
              <select bind:value={gearSlot}>
                {#each gearSlots as slot}
                  <option value={slot}>{slot}</option>
                {/each}
              </select>
            </label>
          </div>

          <div class="segmented">
            <button class:active={gearKind === 'all'} type="button" onclick={() => (gearKind = 'all')}>
              All gear
            </button>
            <button class:active={gearKind === 'armor'} type="button" onclick={() => (gearKind = 'armor')}>
              Armors
            </button>
            <button class:active={gearKind === 'weapon'} type="button" onclick={() => (gearKind = 'weapon')}>
              Weapons
            </button>
          </div>

          <div class="results-meta">
            <strong>{visibleGearCount}</strong>
            <span>matching entries</span>
          </div>

          <div class="table-shell">
            <div class="table-head gear-grid">
              <button type="button" onclick={() => toggleGearSort('name')}>
                Name
              </button>
              <button type="button" onclick={() => toggleGearSort('plugin')}>
                Plugin
              </button>
              <button type="button" onclick={() => toggleGearSort('slot')}>
                Slot / class
              </button>
              <button type="button" onclick={() => toggleGearSort('stat')}>
                Armor / damage
              </button>
              <button type="button" onclick={() => toggleGearSort('weight')}>
                Weight
              </button>
              <button type="button" onclick={() => toggleGearSort('value')}>
                Value
              </button>
            </div>

            <div class="table-body">
              {#each filteredGear as entry}
                <article class="table-row gear-grid">
                  <div class="primary-cell">
                    <div class="title-line">
                      <strong>{entry.name}</strong>
                      <span class:weapon-badge={entry.kind === 'weapon'} class="kind-badge">
                        {entry.kind}
                      </span>
                    </div>
                    <p>{entry.editorId}</p>
                    <div class="tag-row">
                      {#each entry.keywords.slice(0, 3) as keyword}
                        <span>{keyword}</span>
                      {/each}
                    </div>
                  </div>
                  <div>{entry.plugin}</div>
                  <div>
                    <strong>{entry.slot}</strong>
                    <p>{entry.category}</p>
                  </div>
                  <div>{gearStat(entry)}</div>
                  <div>{entry.weight.toFixed(1)}</div>
                  <div>{entry.value}</div>
                </article>
              {/each}
            </div>
          </div>
        </section>
      {:else}
        <section class="browser-panel">
          <div class="toolbar-grid">
            <label class="search-field search-wide">
              <span>Search saved outfits</span>
              <input
                bind:value={outfitSearch}
                type="search"
                placeholder="Name, summary, tag, contained piece"
              />
            </label>

            <label class="search-field">
              <span>Plugin</span>
              <select bind:value={outfitPlugin}>
                {#each outfitPlugins as plugin}
                  <option value={plugin}>{plugin}</option>
                {/each}
              </select>
            </label>

            <label class="search-field">
              <span>Tag</span>
              <select bind:value={outfitTag}>
                {#each outfitTags as tag}
                  <option value={tag}>{tag}</option>
                {/each}
              </select>
            </label>
          </div>

          <div class="results-meta">
            <strong>{visibleOutfitCount}</strong>
            <span>matching outfits</span>
          </div>

          <div class="table-shell">
            <div class="table-head outfit-grid">
              <button type="button" onclick={() => toggleOutfitSort('name')}>
                Outfit
              </button>
              <button type="button" onclick={() => toggleOutfitSort('plugin')}>
                Plugin
              </button>
              <button type="button" onclick={() => toggleOutfitSort('pieces')}>
                Piece count
              </button>
            </div>

            <div class="table-body">
              {#each filteredOutfits as outfit}
                <article class="table-row outfit-grid outfit-row">
                  <div class="primary-cell">
                    <div class="title-line">
                      <strong>{outfit.name}</strong>
                    </div>
                    <p>{outfit.summary}</p>
                    <div class="tag-row">
                      {#each outfit.tags as tag}
                        <span>{tag}</span>
                      {/each}
                    </div>
                    <ul class="piece-list">
                      {#each outfit.pieces as piece}
                        <li>{piece}</li>
                      {/each}
                    </ul>
                  </div>
                  <div>
                    <strong>{outfit.plugin}</strong>
                    <p>{outfit.editorId}</p>
                  </div>
                  <div class="piece-count">{outfit.pieces.length}</div>
                </article>
              {/each}
            </div>
          </div>
        </section>
      {/if}
    </div>
  </section>
</main>
