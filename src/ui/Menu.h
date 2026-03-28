#pragma once

#include "EquipmentCatalog.h"
#include "Keycode.h"
#include "ThemeConfig.h"
#include "VariantWorkbench.h"
#include "components/EquipmentWidget.h"
#include "imgui.h"
#include "ui/ConditionData.h"

#include <array>
#include <optional>
#include <unordered_set>
#include <vector>

struct IDXGISwapChain;
struct ID3D11Device;
struct ID3D11DeviceContext;
namespace SKSE {
class SerializationInterface;
}

namespace sosr {
class MenuHost;

class Menu {
public:
  struct FontOption {
    std::string label;
    std::string path;
    bool isBundled{false};
  };

  static Menu *GetSingleton();

  void Init(IDXGISwapChain *a_swapChain, ID3D11Device *a_device,
            ID3D11DeviceContext *a_context);
  void Draw();
  void Open();
  void Close();
  void Toggle();
  void SetGameDataLoaded(bool a_loaded) { gameDataLoaded_ = a_loaded; }
  [[nodiscard]] bool IsEnabled() const { return enabled_; }
  [[nodiscard]] bool IsInitialized() const { return initialized_; }
  [[nodiscard]] bool IsGameDataLoaded() const { return gameDataLoaded_; }
  [[nodiscard]] bool PauseGameWhenOpen() const { return pauseGameWhenOpen_; }
  [[nodiscard]] bool WantsTextInput() const { return wantTextInput_; }
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
  void SerializeConditions(SKSE::SerializationInterface *a_skse) const;
  void DeserializeConditions(SKSE::SerializationInterface *a_skse);
  void RevertConditions();

private:
  static constexpr const char *kDefaultFontPath =
      "Data/Interface/SkyrimVanitySystem/fonts/Ubuntu-R.ttf";
  static constexpr const char *kDefaultIconFontPath =
      "Data/Interface/SkyrimVanitySystem/fonts/lucide.ttf";
  static constexpr const char *kBundledFontDirectory =
      "Data/Interface/SkyrimVanitySystem/fonts";
  static constexpr int kDefaultFontSizePixels = 16;
  static constexpr int kMinFontSizePixels = 8;
  static constexpr int kMaxFontSizePixels = 48;

  enum class DragSourceKind : std::uint32_t {
    Catalog = 1,
    Override = 2,
    Row = 3,
    SlotCatalog = 4
  };

  struct DraggedEquipmentPayload {
    std::uint32_t sourceKind{0};
    std::int32_t rowIndex{-1};
    std::int32_t itemIndex{-1};
    RE::FormID formID{0};
    std::uint64_t slotMask{0};
  };

  enum class BrowserTab { Gear, Outfits, Kits, Slots, Conditions, Options };
  enum class CatalogRefreshMode : std::uint8_t { Full, KitsOnly };
  enum class VisibilityState : std::uint8_t { Closed, Opening, Open, Closing };
  enum class KitCreationSource : std::uint8_t { Equipped, Overrides };

  struct ConditionEditorState {
    int windowSlot{0};
    std::string sourceConditionId;
    ui::conditions::Definition draft;
    std::string error;
    bool isNew{false};
    bool open{true};
    bool focusOnNextDraw{false};
  };

  Menu() = default;
  friend class MenuHost;

  void ApplyStyle();
  void LoadUserSettings();
  void SaveUserSettings() const;
  void LoadFavorites();
  void SaveFavorites() const;
  void RefreshAvailableFonts();
  void NormalizeSelectedFontPath();
  void RebuildFontAtlas();
  void SyncAllowTextInput();
  void UpdateVisibilityAnimation(float a_deltaTime);
  void QueueHideMessage();
  void ApplySmoothScroll();
  void OnMenuShow();
  void OnMenuHide();
  void HandleCancel();
  void DrawWindow();
  void
  QueueCatalogRefresh(CatalogRefreshMode a_mode = CatalogRefreshMode::Full);
  void UpdateCatalogRefresh();
  void DrawCatalogLoadingPane() const;
  void DrawCatalogFilters();
  [[nodiscard]] sosr::ui::components::EquipmentWidgetResult
  DrawCatalogDragWidget(const workbench::EquipmentWidgetItem &a_item,
                        DragSourceKind a_sourceKind);
  [[nodiscard]] bool DrawGearTab();
  [[nodiscard]] bool
  DrawGearCatalogTable(const std::vector<const GearEntry *> &a_rows);
  void DrawVariantWorkbenchPane();
  [[nodiscard]] bool DrawOutfitTab();
  [[nodiscard]] bool DrawKitTab();
  [[nodiscard]] bool DrawSlotTab();
  [[nodiscard]] bool DrawConditionTab();
  void DrawOptionsTab();
  void DrawConditionEditorDialog();
  void DrawCreateKitDialog();
  void DrawDeleteKitDialog();
  void AcceptOverridePayload(int a_targetRowIndex);
  bool ApplyWorkbenchRowDrop(const DraggedEquipmentPayload &a_dragPayload,
                             int a_targetRowIndex = -1,
                             bool a_insertAfter = false);
  void ApplyRowReorder(const DraggedEquipmentPayload &a_dragPayload,
                       int a_targetRowIndex, bool a_insertAfter);
  void AcceptOverrideDeletePayload();
  void ClearCatalogSelection();
  [[nodiscard]] std::string BuildFavoriteKey(BrowserTab a_tab,
                                             std::string_view a_id) const;
  [[nodiscard]] bool IsFavorite(BrowserTab a_tab, std::string_view a_id) const;
  void SetFavorite(BrowserTab a_tab, std::string_view a_id, bool a_favorite);
  [[nodiscard]] std::string BuildFavoriteLabel(std::string_view a_name,
                                               bool a_favorite) const;
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
  void AddGearEntryToWorkbench(const GearEntry &a_entry);
  void AddOutfitEntryToWorkbench(const OutfitEntry &a_entry,
                                 bool a_replaceExisting = true);
  void AddKitEntryToWorkbench(const KitEntry &a_entry,
                              bool a_replaceExisting = true);
  void OpenCreateKitDialog(KitCreationSource a_source);
  void OpenDeleteKitDialog(const KitEntry &a_entry);
  [[nodiscard]] bool SavePendingKit();
  [[nodiscard]] bool DeletePendingKit();
  void PreviewGearEntry(const GearEntry &a_entry);
  void PreviewOutfitEntry(const OutfitEntry &a_entry);
  void PreviewKitEntry(const KitEntry &a_entry);
  void EnsureDefaultConditions();
  [[nodiscard]] int AllocateConditionEditorWindowSlot() const;
  void OpenNewConditionDialog();
  void OpenConditionEditorDialog(std::size_t a_index);
  void OpenConditionEditorDialogById(std::string_view a_conditionId);
  [[nodiscard]] bool SaveConditionEditor(ConditionEditorState &a_editor);
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
  bool gameDataLoaded_{false};
  bool catalogInitialized_{false};
  bool catalogRefreshQueued_{false};
  CatalogRefreshMode queuedCatalogRefreshMode_{CatalogRefreshMode::Full};
  ID3D11Device *device_{nullptr};
  ID3D11DeviceContext *context_{nullptr};
  BrowserTab activeTab_{BrowserTab::Gear};
  int gearPluginIndex_{0};
  int outfitPluginIndex_{0};
  int kitCollectionIndex_{0};
  std::vector<bool> selectedSlotFilters_;
  int focusedConditionEditorWindowSlot_{0};
  int fontSizePixels_{13};
  int pendingFontSizePixels_{13};
  std::string fontPath_{kDefaultFontPath};
  bool pendingFontAtlasRebuild_{false};
  bool pauseGameWhenOpen_{false};
  bool smoothScroll_{true};
  bool previewSelected_{true};
  bool showAllSlots_{false};
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
  std::string favoritesPath_;
  ImGuiTextFilter gearSearch_;
  ImGuiTextFilter outfitSearch_;
  ImGuiTextFilter kitSearch_;
  ImGuiTextFilter gearPluginFilter_;
  ImGuiTextFilter outfitPluginFilter_;
  ImGuiTextFilter kitCollectionFilter_;
  bool openCreateKitDialog_{false};
  bool createKitDialogOpen_{false};
  bool createKitDialogCancelRequested_{false};
  KitCreationSource createKitSource_{KitCreationSource::Equipped};
  std::vector<RE::FormID> pendingKitFormIDs_;
  std::array<char, 256> pendingKitName_{};
  std::array<char, 256> pendingKitCollection_{};
  std::string createKitError_;
  bool openDeleteKitDialog_{false};
  bool deleteKitDialogOpen_{false};
  bool deleteKitDialogCancelRequested_{false};
  std::string pendingDeleteKitId_;
  std::string pendingDeleteKitName_;
  std::string pendingDeleteKitPath_;
  std::string deleteKitError_;
  int nextConditionId_{1};
  bool favoritesOnly_{false};
  bool inventoryOnly_{false};
  std::string pendingCatalogSelectionAfterRefresh_;
  std::unordered_set<std::string> favoriteKeys_;
  std::vector<FontOption> bundledFontOptions_;
  std::vector<FontOption> systemFontOptions_;
  std::vector<ui::conditions::Definition> conditions_;
  std::vector<ConditionEditorState> conditionEditors_;
  workbench::VariantWorkbench workbench_;
};
} // namespace sosr
