## SVS Refactor Plan

### Goals

- Tighten abstraction boundaries between UI, domain logic, and runtime integration.
- Break large translation units into focused modules with clear ownership.
- Keep the project buildable after each phase.
- Prefer incremental compatibility shims over one-shot rewrites.

### Guiding Rules

- `src/ui/` owns presentation, draft state, and ImGui-specific behavior.
- `src/conditions/` owns persisted condition definitions, validation, graph metadata, and materialization.
- `src/workbench/` or `src/VariantWorkbench*` owns row state and DAV sync, but not UI policy defaults.
- Shared helpers live in dedicated utility modules instead of being reimplemented inside feature files.

### Phase 1: Extract Condition Core Types

Status: in progress

Deliverables:

- Create a core condition module under `src/conditions/`.
- Move condition enums and persisted model types out of `ui/`.
- Replace direct `ImVec4` ownership in the condition model with a UI-agnostic color type.
- Keep `src/ui/ConditionData.h` as a thin compatibility layer during transition.
- Update non-UI code to include core condition headers directly.

Notes:

- This is the seam that unlocks the rest of the refactor.
- Runtime caches can remain attached to the condition definition for now, but they should no longer depend on ImGui types.

### Phase 2: Split Condition Domain from Condition UI

Deliverables:

- Move default condition creation, name/color suggestion, dependency validation, and cycle checks into `src/conditions/`.
- Reduce `src/ui/Menu.Conditions.cpp` to rendering and draft orchestration only.
- Introduce focused UI modules:
  - `src/ui/conditions/ConditionCatalogPane.cpp`
  - `src/ui/conditions/ConditionEditorWindow.cpp`
  - `src/ui/conditions/ConditionClauseTable.cpp`

### Phase 3: Refactor Workbench Boundaries

Deliverables:

- Stop embedding default row-condition policy inside `VariantWorkbench`.
- Make row-condition selection an explicit caller policy.
- Split workbench core logic from DAV persistence/sync logic.
- Keep row ordering and filtered reorder logic in a single focused module.

### Phase 4: Reduce Menu Surface Area

Deliverables:

- Turn `Menu` into a shell that coordinates tabs and lifecycle only.
- Move per-tab/controller logic into dedicated modules:
  - catalog browser
  - workbench pane
  - conditions pane
  - options pane
  - kit dialogs
- Introduce a smaller plain-data state layer where practical.

### Phase 5: Shared Utility Cleanup

Deliverables:

- Consolidate duplicate text helpers.
- Consolidate duplicate form metadata/editor-id/plugin-name helpers.
- Move cross-cutting helpers into dedicated modules with narrow headers.

### Immediate Implementation Order

1. Extract condition core types and convert non-UI code to use them directly.
2. Build and stabilize.
3. Move condition validation/defaults/graph helpers out of `Menu.Conditions.cpp`.
4. Split workbench policy from workbench storage/sync.
5. Shrink `Menu` after the lower-level seams are established.
