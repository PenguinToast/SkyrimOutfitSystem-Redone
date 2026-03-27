#pragma once

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace sosr::workbench {
enum class EquipmentWidgetItemKind : std::uint8_t {
  Armor,
  Slot
};

struct EquipmentWidgetItem {
  EquipmentWidgetItemKind kind{EquipmentWidgetItemKind::Armor};
  RE::FormID formID{0};
  std::string key;
  std::string name;
  std::string slotText;
  std::uint64_t slotMask{0};

  [[nodiscard]] bool IsSlot() const {
    return kind == EquipmentWidgetItemKind::Slot;
  }

  [[nodiscard]] bool HasForm() const { return formID != 0; }

  [[nodiscard]] bool SupportsInfoTooltip() const {
    return !IsSlot() && HasForm();
  }
};

struct VariantWorkbenchRow {
  std::string key;
  EquipmentWidgetItem equipped;
  std::vector<EquipmentWidgetItem> overrides;
  bool hideEquipped{false};
  bool isEquipped{false};

  [[nodiscard]] bool IsSlotRow() const {
    return equipped.IsSlot();
  }

  [[nodiscard]] bool IsVisualConflictSource() const {
    return IsSlotRow() || isEquipped;
  }

  [[nodiscard]] std::uint64_t GetSelectionConflictSlotMask() const;

  [[nodiscard]] std::uint64_t
  GetOverrideVisualSlotMask(const EquipmentWidgetItem &a_item) const {
    return a_item.slotMask;
  }
};

class VariantWorkbench {
public:
  void SyncRowsFromPlayer();
  bool BuildCatalogItem(RE::FormID a_formID, EquipmentWidgetItem &a_item) const;
  bool BuildSlotItem(std::uint64_t a_slotMask, EquipmentWidgetItem &a_item) const;
  [[nodiscard]] bool
  IsPreviewingSelection(std::string_view a_selectionKey) const;
  [[nodiscard]] bool CanAcceptOverride(int a_targetRowIndex,
                                       const EquipmentWidgetItem &a_item,
                                       int a_sourceRowIndex = -1,
                                       int a_sourceItemIndex = -1) const;
  bool AddCatalogOverride(int a_targetRowIndex, RE::FormID a_formID);
  bool AddCatalogSelectionToWorkbench(const std::vector<RE::FormID> &a_formIDs);
  bool ReplaceCatalogSelectionInWorkbench(
      const std::vector<RE::FormID> &a_formIDs);
  bool AddCatalogSelectionAsRows(const std::vector<RE::FormID> &a_formIDs);
  bool AddSlotRow(std::uint64_t a_slotMask);
  bool ApplyCatalogPreview(std::string_view a_selectionKey,
                           const std::vector<RE::FormID> &a_formIDs);
  void ClearPreview();
  bool MoveOverride(int a_sourceRowIndex, int a_sourceItemIndex,
                    int a_targetRowIndex);
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
  bool DeleteOverride(int a_rowIndex, int a_itemIndex);
  bool DeleteRow(int a_rowIndex);
  bool SetHideEquipped(int a_rowIndex, bool a_hideEquipped);
  bool ResetEquippedRows();
  void ResetAllRows();
  [[nodiscard]] std::vector<RE::FormID> CollectEquippedArmorFormIDs() const;
  [[nodiscard]] std::vector<RE::FormID>
  CollectOverrideArmorFormIDsFromEquippedRows() const;
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
  bool InsertCatalogRow(RE::FormID a_formID, int a_targetRowIndex,
                        bool a_insertAfter);
  bool InsertSlotRow(std::uint64_t a_slotMask, int a_targetRowIndex,
                     bool a_insertAfter);
  bool ApplyRowReorder(int a_sourceRowIndex, int a_targetRowIndex,
                       bool a_insertAfter);
  void SyncDynamicArmorVariantsExtended();
  void Serialize(SKSE::SerializationInterface *a_skse) const;
  void Deserialize(SKSE::SerializationInterface *a_skse);
  void Revert();

  [[nodiscard]] const std::vector<VariantWorkbenchRow> &GetRows() const {
    return rows_;
  }
  [[nodiscard]] std::size_t GetRowCount() const { return rows_.size(); }

private:
  struct PlannedCatalogAssignment {
    int rowIndex{-1};
    RE::FormID armorFormID{0};
  };

  [[nodiscard]] bool
  ResolveCatalogArmors(const std::vector<RE::FormID> &a_formIDs,
                       std::vector<const RE::TESObjectARMO *> &a_armors) const;
  [[nodiscard]] int FindBestCatalogTargetRowIndex(
      const EquipmentWidgetItem &a_item, bool a_requireAcceptable,
      const std::vector<PlannedCatalogAssignment> *a_pendingAssignments) const;
  [[nodiscard]] bool CanAcceptOverrideWithPendingAssignments(
      int a_targetRowIndex, const EquipmentWidgetItem &a_item,
      const std::vector<PlannedCatalogAssignment> &a_pendingAssignments) const;
  [[nodiscard]] bool PlanCatalogAssignments(
      const std::vector<RE::FormID> &a_formIDs,
      std::vector<PlannedCatalogAssignment> &a_assignments) const;
  [[nodiscard]] std::vector<VariantWorkbenchRow>
  BuildCatalogRows(const std::vector<RE::FormID> &a_formIDs) const;
  [[nodiscard]] std::optional<VariantWorkbenchRow>
  BuildSlotRow(std::uint64_t a_slotMask) const;
  [[nodiscard]] int
  FindBestCatalogTargetRowIndex(const EquipmentWidgetItem &a_item,
                                bool a_requireAcceptable) const;
  void RebuildRowOrder();

  std::vector<VariantWorkbenchRow> rows_;
  std::vector<std::string> rowOrder_;
  std::unordered_map<std::string, std::string> activeDavVariants_;
  std::string previewSelectionKey_;
  std::unordered_map<std::string, std::string> previewDavVariants_;
};
} // namespace sosr::workbench
