## SVS Refactor Plan 3

### Goals

- Keep shrinking `Menu` from a broad coordinator into a thinner shell over
  pane-specific state and commands.
- Make catalog UI hosting explicitly separate from workbench hosting so a future
  popout window is a hosting concern instead of a deep refactor.
- Reduce the remaining UI-adapter leakage from `VariantWorkbench` by moving
  widget-item construction and adapter types behind dedicated workbench modules.
- Continue using buildable, commit-sized slices with validation after each
  phase.

### Remaining Context

1. `Menu.h` is still a broad coordinator façade. The implementation has been
   split, but pane-specific state types still live there and the menu still owns
   too much of the state vocabulary directly.
2. Catalog browser state is isolated, but the host concept is still implicit.
   We want the catalog side to be separable enough that a future popout window
   can render the same browser/condition tabs without first undoing ownership.
3. `VariantWorkbench` still exposes widget-adapter-oriented types and builders,
   especially `EquipmentWidgetItem` and the item-construction helpers used by UI
   and serialization.
4. The remaining worthwhile refactors are now mostly type-boundary refactors
   rather than large translation-unit splits.

### Guiding Rules

- `src/ui/catalog/` should own catalog-pane host and browser state.
- `src/ui/workbench/` should own workbench-pane state and filter vocabulary.
- `src/ui/conditions/` should own condition-editor UI state types.
- `src/workbench/` should own widget-adapter item types/factories that bridge
  row data to UI widgets, rather than `VariantWorkbench` itself owning them.
- `Menu` should host pane state and coordinate between panes, but avoid defining
  pane-specific state structs inline where possible.

### Phase 1: Extract Pane State Types From Menu

Deliverables:

- Move `ConditionEditorState` into a dedicated condition UI header.
- Move workbench filter state/option types into a dedicated workbench UI header.
- Replace inline `Menu` state type definitions with imports/aliases from those
  modules.
- Keep behavior unchanged while shrinking `Menu.h`'s local vocabulary.

### Phase 2: Introduce a Popout-Ready Catalog Host State

Deliverables:

- Replace the bare catalog browser field with a dedicated catalog pane/host
  state type under `src/ui/catalog/`.
- Include future-facing host metadata there, such as docked/popout host mode and
  popout-open state, even if the popout window itself is not implemented yet.
- Route menu-side catalog access through that host state instead of directly
  owning the browser state as a top-level field.

### Phase 3: Extract Workbench Item Types And Item Factory

Deliverables:

- Move `EquipmentWidgetItemKind` and `EquipmentWidgetItem` out of
  `VariantWorkbench.h` into a dedicated workbench item header.
- Move `BuildCatalogItem(...)` and `BuildSlotItem(...)` into a dedicated
  workbench item-factory module.
- Update UI/serialization code to use the extracted item factory instead of
  asking `VariantWorkbench` to build widget-facing items.
- Keep `VariantWorkbench` focused on row storage, row mutation, preview, sync,
  and serialization.

### Validation Policy

- After each phase:
  1. build and deploy with `./scripts/build-deploy.sh releasedbg`
  2. fix any fallout in the same slice
  3. commit before starting the next phase

### Final Review Checklist

- `Menu.h` should define fewer pane-specific types inline.
- Catalog hosting should be ready for a later popout without another ownership
  rewrite.
- `VariantWorkbench.h` should no longer be the source of widget-item type
  definitions or item-construction helpers.
- Residual broad coordination in `Menu` should be documented explicitly if it
  remains.
