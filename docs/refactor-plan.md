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

Status: completed

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

Status: completed

Deliverables:

- Move default condition creation, name/color suggestion, dependency validation, and cycle checks into `src/conditions/`.
- Reduce `src/ui/Menu.Conditions.cpp` to rendering and draft orchestration only.
- Introduce focused UI modules:
  - `src/ui/conditions/ConditionCatalogPane.cpp`
  - `src/ui/conditions/ConditionEditorWindow.cpp`
  - `src/ui/conditions/ConditionClauseTable.cpp`

Progress:

- Completed:
  - `src/conditions/Definition.h`
  - `src/conditions/Defaults.cpp`
  - `src/conditions/Validation.cpp`
  - `src/ui/conditions/EditorSupport.cpp`
  - `src/ui/conditions/Widgets.cpp`
  - `src/ui/Menu.Conditions.State.cpp`
  - `src/ui/Menu.Conditions.Catalog.cpp`
  - `src/ui/Menu.Conditions.Editor.cpp`
  - `src/ui/Menu.Conditions.ClauseTable.cpp`
  - `src/ui/Menu.Conditions.Serialization.cpp`
- Remaining:
  - none for this phase; the current `Menu.Conditions.*.cpp` split is the stable module layout

### Phase 3: Refactor Workbench Boundaries

Status: completed

Deliverables:

- Stop embedding default row-condition policy inside `VariantWorkbench`.
- Make row-condition selection an explicit caller policy.
- Split workbench core logic from DAV persistence/sync logic.
- Keep row ordering and filtered reorder logic in a single focused module.

Progress:

- Completed:
  - `VariantWorkbench` no longer silently defaults missing new-row conditions to `Player`
  - caller-side policy now lives in menu/workbench filter code
  - workbench filter/default-target logic moved out of `Menu.cpp` into `src/ui/Menu.Workbench.Filters.cpp`
  - DAV preview/sync and save-load logic split into:
    - `src/VariantWorkbench.DavSync.cpp`
    - `src/VariantWorkbench.Serialization.cpp`
  - row storage/manipulation remains in `src/VariantWorkbench.cpp`
Notes:

- The workbench boundary is now stable enough that an additional `src/workbench/` directory move would be cosmetic rather than architectural.

### Phase 4: Reduce Menu Surface Area

Status: completed

Deliverables:

- Turn `Menu` into a shell that coordinates tabs and lifecycle only.
- Move per-tab/controller logic into dedicated modules:
  - catalog browser
  - workbench pane
  - conditions pane
  - options pane
  - kit dialogs
- Introduce a smaller plain-data state layer where practical.

Completed:

- `Menu.cpp` is now a small shell/entry unit.
- Lifecycle/settings/browser/workbench/conditions/catalog tab logic now live in dedicated modules:
  - `src/ui/Menu.Visibility.cpp`
  - `src/ui/Menu.Settings.cpp`
  - `src/ui/Menu.Browser.cpp`
  - `src/ui/Menu.Workbench.cpp`
  - `src/ui/Menu.Workbench.Filters.cpp`
  - `src/ui/Menu.Gear.cpp`
  - `src/ui/Menu.Outfits.cpp`
  - `src/ui/Menu.Kits.cpp`
  - `src/ui/Menu.Slots.cpp`
  - `src/ui/Menu.Options.cpp`
  - `src/ui/Menu.Conditions.*.cpp`

Notes:

- `Menu.h` remains a broad coordinating facade, but the implementation surface is no longer concentrated in one translation unit.

### Phase 5: Shared Utility Cleanup

Status: completed

Deliverables:

- Consolidate duplicate text helpers.
- Consolidate duplicate form metadata/editor-id/plugin-name helpers.
- Move cross-cutting helpers into dedicated modules with narrow headers.

Completed:

- Shared text helpers were consolidated into `src/StringUtils.h`.
- `EquipmentCatalog.cpp` now uses `ArmorUtils` for shared form metadata helpers instead of carrying its own duplicate plugin/editor-id/display-name implementations.
- Input/menu coupling was narrowed through `src/ui/InputSinkBridge.cpp`.

### Immediate Implementation Order

1. Extract condition core types and convert non-UI code to use them directly.
2. Build and stabilize.
3. Move condition validation/defaults/graph helpers out of `Menu.Conditions.cpp`.
4. Split workbench policy from workbench storage/sync.
5. Shrink `Menu` after the lower-level seams are established.


### Context

 1. Menu is a god object that currently owns too many unrelated subsystems, which makes every feature change cross-cut multiple tabs and increases regression risk. The same class owns
     menu lifecycle, input capture, font/theme settings, catalog filtering, workbench filtering/sync, kit CRUD, and condition editing in one header and one implementation surface:
     SkyrimVanitySystem/src/ui/Menu.h:26, SkyrimVanitySystem/src/ui/Menu.h:137, SkyrimVanitySystem/src/ui/Menu.cpp:729, SkyrimVanitySystem/src/ui/Menu.cpp:851, SkyrimVanitySystem/src/
     ui/Menu.cpp:1190, SkyrimVanitySystem/src/ui/Menu.cpp:1406. This is the main place to refactor first. A better boundary would be: MenuShell for lifecycle/frame orchestration,
     CatalogBrowserController, WorkbenchController, ConditionController, and OptionsController, with per-tab rendering moved out of Menu entirely.
  2. The condition model is not a true domain model yet; it is still UI-coupled and execution-coupled. ui::conditions::Definition stores an ImVec4 color and also stores transient
     materialization/runtime cache state (shared_ptr<RE::TESCondition>, signatures, refresh targets), while non-UI code consumes it directly: SkyrimVanitySystem/src/ui/
     ConditionData.h:16, SkyrimVanitySystem/src/ui/ConditionData.h:39, SkyrimVanitySystem/src/ConditionMaterializer.cpp:21, SkyrimVanitySystem/src/VariantWorkbench.cpp:44. This makes
     the condition core depend on ImGui/UI types and forces workbench/materializer code to reach into ui::conditions. I would split this into:

  - conditions/Definition.h: pure persisted model
  - conditions/GraphMetadata.h: refs/reverse-deps

  3. VariantWorkbench still embeds UI/policy decisions that should be injected from above. The workbench core decides what the default condition is and uses the UI constant directly,
     then uses that during actor sync and row creation: SkyrimVanitySystem/src/VariantWorkbench.cpp:44, SkyrimVanitySystem/src/VariantWorkbench.cpp:53, SkyrimVanitySystem/src/
     VariantWorkbench.cpp:301. That means the workbench is not just a row/override model; it also knows SVS default condition policy. This should move behind a passed-in
     RowConditionPolicy or explicit condition argument everywhere. If a caller wants default Player, it should decide that before calling the workbench.
  4. Menu.Conditions.cpp is doing four jobs at once: defaults/factories, validation/graph rules, function/parameter metadata shaping, and the full editor/catalog rendering. The top of
     the file already contains domain-ish logic like default condition creation and naming/color selection: SkyrimVanitySystem/src/ui/Menu.Conditions.cpp:230, SkyrimVanitySystem/src/
     ui/Menu.Conditions.cpp:240, SkyrimVanitySystem/src/ui/Menu.Conditions.cpp:303. Then the same file contains the entire editor window and clause table renderer: SkyrimVanitySystem/
     src/ui/Menu.Conditions.cpp:1388, SkyrimVanitySystem/src/ui/Menu.Conditions.cpp:1494, SkyrimVanitySystem/src/ui/Menu.Conditions.cpp:1891. This file is large enough now that
     refactoring by feature is warranted, not optional. I would split into:

  - ConditionDefaults.cpp
  - ConditionDraftValidation.cpp
  - ConditionCatalogPane.cpp
     EditableCombo.cpp:14, SkyrimVanitySystem/src/ConditionMaterializer.cpp:48. More importantly, EquipmentCatalog.cpp duplicates form/editor/plugin/display helpers that already exist
     in ArmorUtils.cpp: SkyrimVanitySystem/src/ArmorUtils.cpp:93, SkyrimVanitySystem/src/ArmorUtils.cpp:117, SkyrimVanitySystem/src/ArmorUtils.cpp:178, SkyrimVanitySystem/src/
     EquipmentCatalog.cpp:77, SkyrimVanitySystem/src/EquipmentCatalog.cpp:110, SkyrimVanitySystem/src/EquipmentCatalog.cpp:127. That should be consolidated before it drifts further.
  6. InputManager still depends directly on Menu::GetSingleton() and menu state to decide how keyboard events are interpreted: SkyrimVanitySystem/src/InputManager.cpp:227,
     SkyrimVanitySystem/src/InputManager.cpp:233. That makes the input backend know about one specific UI consumer instead of exposing a small input sink / wants text input
     abstraction. It works, but it is another coupling point that will make future UI decomposition harder.

### Final Review Against Context

1. `Menu` implementation is no longer a single god translation unit.
   - Addressed by the `Menu.*.cpp` split.
   - Residual: `Menu.h` still exposes a large facade, but that is now coordination surface rather than one monolithic implementation file.

2. Condition definitions are now a true persisted model.
   - Addressed by `src/conditions/Definition.h`, `src/conditions/GraphMetadata.h`, and `src/conditions/MaterializationState.h`.
   - Runtime materialization/cache state no longer lives on the persisted definition.

3. `VariantWorkbench` no longer owns default UI condition policy.
   - Addressed by caller-side condition resolution in menu/workbench filter code.

4. `Menu.Conditions.cpp` no longer mixes all condition responsibilities.
   - Addressed by the `Menu.Conditions.*.cpp` split and `src/ui/conditions/*`.

5. Duplicate helper logic has been reduced at the shared-module boundary.
   - Addressed by `StringUtils.h` and the `EquipmentCatalog` -> `ArmorUtils` consolidation.

6. `InputManager` no longer depends directly on `Menu::GetSingleton()`.
   - Addressed by `src/ui/InputSinkBridge.*`.
