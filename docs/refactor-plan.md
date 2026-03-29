## SVS Refactor Plan 2

### Goals

- Make catalog UI ownership distinct from workbench UI ownership.
- Keep future catalog popout support straightforward by isolating catalog state
  and behavior behind dedicated modules.
- Tighten the remaining abstraction boundaries between condition domain logic,
  condition editor UI, workbench core logic, and DAV/runtime integration.
- Continue using buildable, low-risk slices with validation after each phase.

### Guiding Rules

- `src/ui/catalog/` owns catalog/browser state and catalog-pane behavior.
- `src/ui/workbench/` or `src/ui/Menu.Workbench.*` owns workbench-pane
  rendering, tooltips, drag/drop, and toolbar behavior, but not catalog state.
- `src/conditions/` owns condition definitions, dependency graph metadata,
  graph validation, and condition-materialization orchestration.
- Condition editor UI modules should depend on condition-domain services, not
  re-own them.
- `VariantWorkbench` owns row/core state and DAV integration entry points, but
  should not expose more UI adapter vocabulary than necessary.

### Remaining Context

1. `Menu` is still a broad controller facade even though the implementation has
   been split. `src/ui/Menu.h` still owns browser tab state, workbench filter
   state, condition editor draft state, kit dialog state, drag payload types,
   and tab/controller methods in one class.
2. `VariantWorkbench` still mixes row-domain behavior with UI adapter concerns
   and DAV preview/sync behavior. It exposes row storage, widget-facing item
   structures, catalog/slot item construction, preview, sync, and serialization
   from one class boundary.
3. `ConditionMaterializer` still combines editor-facing parameter coercion,
   boolean-algebra lowering, dependency graph/cache invalidation, and TESCondition
   emission in one module.
4. `Menu.Workbench.cpp` is still a large mixed-responsibility UI module
   containing toolbar policy, tooltip helpers, drag/drop command handling, row
   rendering, reorder-preview geometry, and conflict highlighting.
5. `ui/conditions/EditorSupport.cpp` still combines new-condition defaults,
   condition function discovery/metadata, draft validation, formatting helpers,
   and ImGui editor widgets.
6. `EquipmentCatalog.cpp` still owns too much of the catalog pipeline in one
   file: collection-tree shaping, search token construction, leveled-list
   recursion/caching, outfit description building, kit parsing, entry building,
   and refresh orchestration.
7. Catalog UI should be owned separately enough from workbench UI that it can
   eventually be rendered in its own popout window with the same Gear/Outfits/
   Kits/Slots/Conditions tab set.

### Phase 1: Extract Catalog Browser Ownership

Status: completed

Deliverables:

- Introduce a dedicated catalog-browser state type under `src/ui/catalog/`.
- Move catalog-specific state out of `Menu` and into that type:
  - active browser tab
  - catalog refresh/selection state
  - favorites state
  - catalog filters/search fields
  - preview-selected state
  - inventory-only / show-all toggles
- Keep `Menu` as a host/coordinator that owns a `CatalogBrowserState` instance
  instead of individually owning those fields.
- Preserve current behavior while making future catalog popout hosting simpler.

Completed:

- Added `src/ui/catalog/BrowserState.h`.
- Moved catalog browser state out of `Menu`'s field list into
  `catalogBrowser_`.
- Updated catalog browser/filter/favorites code to flow through that state.

### Phase 2: Separate Workbench UI Responsibilities

Status: completed

Deliverables:

- Split `Menu.Workbench.cpp` into focused modules:
  - toolbar/actions
  - row table rendering
  - tooltip/drag-drop helpers
- Keep workbench filter policy separate from catalog browser state.
- Ensure the workbench pane depends on explicit catalog/workbench bridge inputs,
  not direct catalog-state ownership.

Completed:

- Split the monolithic workbench pane into:
  - `src/ui/Menu.Workbench.cpp` orchestration and payload handling
  - `src/ui/Menu.Workbench.Toolbar.cpp`
  - `src/ui/Menu.Workbench.Table.cpp`
  - `src/ui/workbench/Common.h`
  - `src/ui/workbench/Tooltips.h`
  - `src/ui/workbench/Tooltips.cpp`

### Phase 3: Split Condition Editor Support Modules

Status: completed

Deliverables:

- Split `ui/conditions/EditorSupport.cpp` into:
  - condition function metadata/registry
  - draft validation helpers
  - value editor widgets / numeric formatting helpers
- Keep a thin compatibility façade only if needed.

Completed:

- Replaced `EditorSupport.cpp` with:
  - `src/ui/conditions/FunctionRegistry.cpp`
  - `src/ui/conditions/DraftValidation.cpp`
  - `src/ui/conditions/ValueEditors.cpp`
- Kept `src/ui/conditions/EditorSupport.h` as an umbrella compatibility header.

### Phase 4: Split Condition Materialization Responsibilities

Status: completed

Deliverables:

- Split `ConditionMaterializer` into separate modules for:
  - graph metadata / reverse dependencies / cache invalidation
  - CNF lowering / boolean algebra
  - TESCondition emission / signature / refresh target generation
- Keep the existing public API stable where practical.

Completed:

- `src/ConditionMaterializer.cpp` now owns cache orchestration only.
- Lowering and emission moved into:
  - `src/conditions/Lowering.h`
  - `src/conditions/Lowering.cpp`

### Phase 5: Reduce EquipmentCatalog Breadth

Status: completed

Deliverables:

- Split `EquipmentCatalog.cpp` into focused helpers/modules for:
  - entry/search text builders
  - outfit/leveled-list description building
  - kit parsing
  - refresh orchestration
- Keep `EquipmentCatalog` itself as the public façade.

Completed:

- Moved entry/search/description builder logic into:
  - `src/catalog/EntryBuilders.h`
  - `src/catalog/EntryBuilders.cpp`
- `src/EquipmentCatalog.cpp` is now primarily the façade for refresh,
  indexing, and derived option rebuilding.

### Validation Policy

- After each phase:
  1. build and deploy with `./scripts/build-deploy.sh releasedbg`
  2. run formatter/lint as needed for the touched slice
  3. commit the slice before continuing

### Execution Order

1. Extract catalog browser ownership.
2. Split workbench UI rendering helpers.
3. Split condition editor support helpers.
4. Split condition materialization internals.
5. Split EquipmentCatalog breadth where still needed.
6. Review this plan against the final code shape and update statuses/notes.

### Final Review

1. Catalog UI ownership is materially cleaner.
   - Catalog browser state is now isolated in `src/ui/catalog/BrowserState.h`.
   - This is enough to support a future catalog popout without first undoing
     workbench-specific ownership.

2. Workbench UI responsibilities are now separated.
   - The main workbench pane no longer owns toolbar, table, and tooltip logic in
     one translation unit.

3. Condition editor support is no longer a kitchen-sink helper layer.
   - Metadata/lookup, draft validation, and value editors now live in distinct
     modules.

4. Condition materialization now has a real internal boundary.
   - Cache/dependency orchestration and lowering/emission are split.

5. Equipment catalog breadth is reduced.
   - The public catalog façade is now narrower, with helper-heavy entry building
     moved behind a dedicated module.

### Residual Notes

- `Menu.h` is still a broad coordinating façade. The major implementation-level
  god-object pressure has been reduced, but the remaining state surface is still
  centralized by design.
- `VariantWorkbench` still exposes widget-adapter-oriented item types and build
  helpers. That is the next natural seam if we later want a deeper domain/UI
  split there, but it is no longer blocking the catalog/workbench ownership
  boundary or the popout direction.
