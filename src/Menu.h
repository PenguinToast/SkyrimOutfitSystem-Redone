#pragma once

#include "EquipmentCatalog.h"
#include "Keycode.h"
#include "ThemeConfig.h"
#include "VariantWorkbench.h"
#include "imgui.h"

struct IDXGISwapChain;
struct ID3D11Device;
struct ID3D11DeviceContext;

namespace sosr {
class MenuHost;

class Menu {
public:
  static Menu *GetSingleton();

  void Init(IDXGISwapChain *a_swapChain, ID3D11Device *a_device,
            ID3D11DeviceContext *a_context);
  void Draw();
  void Open();
  void Close();
  void Toggle();
  [[nodiscard]] bool IsEnabled() const { return enabled_; }
  [[nodiscard]] bool IsInitialized() const { return initialized_; }
  [[nodiscard]] bool PauseGameWhenOpen() const { return pauseGameWhenOpen_; }
  [[nodiscard]] bool QueueSmoothScroll(float a_deltaY);
  [[nodiscard]] std::string GetToggleKeyLabel() const;
  [[nodiscard]] std::uint32_t GetToggleKey() const { return toggleKey_; }
  [[nodiscard]] std::uint32_t GetToggleModifier() const {
    return toggleModifier_;
  }
  [[nodiscard]] bool IsCapturingToggleKey() const {
    return awaitingToggleKeyCapture_;
  }
  void OpenToggleKeyCapture();
  void CloseToggleKeyCapture();
  void HandleToggleKeyCapture(std::uint32_t a_scanCode,
                              std::uint32_t a_modifierScanCode);
  [[nodiscard]] workbench::VariantWorkbench &GetWorkbench() {
    return workbench_;
  }

private:
  enum class DragSourceKind : std::uint32_t {
    Catalog = 1,
    Override = 2,
    Row = 3
  };

  struct DraggedEquipmentPayload {
    std::uint32_t sourceKind{0};
    std::int32_t rowIndex{-1};
    std::int32_t itemIndex{-1};
    RE::FormID formID{0};
  };

  enum class BrowserTab { Gear, Outfits, Kits, Options };
  enum class VisibilityState : std::uint8_t { Closed, Opening, Open, Closing };

  Menu() = default;
  friend class MenuHost;

  void ApplyStyle();
  void LoadUserSettings();
  void SaveUserSettings() const;
  void RebuildFontAtlas();
  void SyncAllowTextInput();
  void UpdateVisibilityAnimation(float a_deltaTime);
  void QueueHideMessage();
  void ApplySmoothScroll();
  void OnMenuShow();
  void OnMenuHide();
  void DrawWindow();
  void DrawCatalogFilters();
  [[nodiscard]] bool DrawGearTab();
  [[nodiscard]] bool
  DrawGearCatalogTable(const std::vector<const GearEntry *> &a_rows);
  void DrawVariantWorkbenchPane();
  [[nodiscard]] bool DrawOutfitTab();
  [[nodiscard]] bool DrawKitTab();
  void DrawOptionsTab();
  bool DrawSearchableStringCombo(const char *a_label, const char *a_allLabel,
                                 const std::vector<std::string> &a_options,
                                 int &a_index, ImGuiTextFilter &a_filter);
  bool DrawEquipmentInfoWidget(const char *a_id,
                               const workbench::EquipmentWidgetItem &a_item,
                               bool a_allowDrag, DragSourceKind a_sourceKind,
                               bool a_showDeleteButton = false,
                               int a_rowIndex = -1, int a_itemIndex = -1,
                               bool a_conflict = false);
  void AcceptOverridePayload(int a_targetRowIndex);
  void ApplyRowReorder(const DraggedEquipmentPayload &a_dragPayload,
                       int a_targetRowIndex, bool a_insertAfter);
  void AcceptOverrideDeletePayload();
  void ClearCatalogSelection();
  void SyncSelectedSlotFilters();
  [[nodiscard]] bool HasAnySelectedSlotFilter() const;
  [[nodiscard]] bool
  MatchesSelectedSlotsOr(const std::vector<std::string> &a_slots) const;
  [[nodiscard]] bool
  MatchesSelectedSlotsAnd(const std::vector<std::string> &a_slots) const;
  [[nodiscard]] std::string BuildSelectedSlotPreview() const;

  [[nodiscard]] bool MatchesGearFilters(const GearEntry &a_entry) const;
  [[nodiscard]] bool MatchesOutfitFilters(const OutfitEntry &a_entry) const;
  [[nodiscard]] bool MatchesKitFilters(const KitEntry &a_entry) const;
  [[nodiscard]] std::vector<const GearEntry *> BuildFilteredGear() const;
  [[nodiscard]] std::vector<const OutfitEntry *> BuildFilteredOutfits() const;
  [[nodiscard]] std::vector<const KitEntry *> BuildFilteredKits() const;
  void SortGearRows(std::vector<const GearEntry *> &a_rows,
                    ImGuiTableSortSpecs *a_sortSpecs) const;
  void SortOutfitRows(std::vector<const OutfitEntry *> &a_rows,
                      ImGuiTableSortSpecs *a_sortSpecs) const;
  void SortKitRows(std::vector<const KitEntry *> &a_rows,
                   ImGuiTableSortSpecs *a_sortSpecs) const;

  bool initialized_{false};
  bool enabled_{false};
  ID3D11Device *device_{nullptr};
  ID3D11DeviceContext *context_{nullptr};
  BrowserTab activeTab_{BrowserTab::Gear};
  int gearPluginIndex_{0};
  int outfitPluginIndex_{0};
  int kitCollectionIndex_{0};
  std::vector<bool> selectedSlotFilters_;
  int fontSizePixels_{13};
  int pendingFontSizePixels_{13};
  bool pendingFontAtlasRebuild_{false};
  bool pauseGameWhenOpen_{false};
  bool smoothScroll_{true};
  bool previewSelected_{true};
  std::uint32_t toggleKey_{0x40};
  std::uint32_t toggleModifier_{0};
  std::string selectedCatalogKey_;
  std::string themeName_{"default"};
  std::string toggleKeyCaptureError_;
  bool awaitingToggleKeyCapture_{false};
  bool openToggleKeyPopup_{false};
  bool wantTextInput_{false};
  bool hideMessageQueued_{false};
  VisibilityState visibilityState_{VisibilityState::Closed};
  float windowAlpha_{0.0f};
  float pendingSmoothWheelDelta_{0.0f};
  ImGuiID smoothScrollWindowId_{0};
  float smoothScrollTargetY_{0.0f};
  std::string settingsDirectory_;
  std::string imguiIniPath_;
  std::string userSettingsPath_;
  ImGuiTextFilter gearSearch_;
  ImGuiTextFilter outfitSearch_;
  ImGuiTextFilter kitSearch_;
  ImGuiTextFilter gearPluginFilter_;
  ImGuiTextFilter outfitPluginFilter_;
  ImGuiTextFilter kitCollectionFilter_;
  workbench::VariantWorkbench workbench_;
};
} // namespace sosr
