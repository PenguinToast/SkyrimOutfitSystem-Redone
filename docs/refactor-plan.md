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

Status: in progress

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

### Phase 2: Separate Workbench UI Responsibilities

Deliverables:

- Split `Menu.Workbench.cpp` into focused modules:
  - toolbar/actions
  - row table rendering
  - tooltip/drag-drop helpers
- Keep workbench filter policy separate from catalog browser state.
- Ensure the workbench pane depends on explicit catalog/workbench bridge inputs,
  not direct catalog-state ownership.

### Phase 3: Split Condition Editor Support Modules

Deliverables:

- Split `ui/conditions/EditorSupport.cpp` into:
  - condition function metadata/registry
  - draft validation helpers
  - value editor widgets / numeric formatting helpers
- Keep a thin compatibility façade only if needed.

### Phase 4: Split Condition Materialization Responsibilities

Deliverables:

- Split `ConditionMaterializer` into separate modules for:
  - graph metadata / reverse dependencies / cache invalidation
  - CNF lowering / boolean algebra
  - TESCondition emission / signature / refresh target generation
- Keep the existing public API stable where practical.

### Phase 5: Reduce EquipmentCatalog Breadth

Deliverables:

- Split `EquipmentCatalog.cpp` into focused helpers/modules for:
  - entry/search text builders
  - outfit/leveled-list description building
  - kit parsing
  - refresh orchestration
- Keep `EquipmentCatalog` itself as the public façade.

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
