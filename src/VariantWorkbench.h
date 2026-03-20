#pragma once

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <string>
#include <unordered_map>
#include <vector>

namespace sosr::workbench {
struct EquipmentWidgetItem {
  RE::FormID formID{0};
  std::string key;
  std::string name;
  std::string slotText;
  std::uint64_t slotMask{0};
};

struct VariantWorkbenchRow {
  std::string key;
  EquipmentWidgetItem equipped;
  std::vector<EquipmentWidgetItem> overrides;
  bool hideEquipped{false};
  bool isEquipped{false};
};

class VariantWorkbench {
public:
  void SyncRowsFromPlayer();
  bool BuildCatalogItem(RE::FormID a_formID, EquipmentWidgetItem &a_item) const;
  [[nodiscard]] bool IsPreviewingForm(RE::FormID a_formID) const;
  [[nodiscard]] bool CanAcceptOverride(int a_targetRowIndex,
                                       const EquipmentWidgetItem &a_item,
                                       int a_sourceRowIndex = -1,
                                       int a_sourceItemIndex = -1) const;
  bool AddCatalogOverride(int a_targetRowIndex, RE::FormID a_formID);
  bool AddCatalogSelectionToWorkbench(RE::FormID a_formID);
  bool ApplyCatalogPreview(RE::FormID a_formID);
  void ClearPreview();
  bool MoveOverride(int a_sourceRowIndex, int a_sourceItemIndex,
                    int a_targetRowIndex);
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
  bool DeleteOverride(int a_rowIndex, int a_itemIndex);
  bool DeleteRow(int a_rowIndex);
  bool SetHideEquipped(int a_rowIndex, bool a_hideEquipped);
  bool ResetEquippedRows();
  void ResetAllRows();
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
  bool InsertCatalogRow(RE::FormID a_formID, int a_targetRowIndex,
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
  ResolveCatalogArmors(RE::FormID a_formID,
                       std::vector<const RE::TESObjectARMO *> &a_armors) const;
  [[nodiscard]] int FindBestCatalogTargetRowIndex(
      const EquipmentWidgetItem &a_item, bool a_requireAcceptable,
      const std::vector<PlannedCatalogAssignment> *a_pendingAssignments) const;
  [[nodiscard]] bool CanAcceptOverrideWithPendingAssignments(
      int a_targetRowIndex, const EquipmentWidgetItem &a_item,
      const std::vector<PlannedCatalogAssignment> &a_pendingAssignments) const;
  [[nodiscard]] bool PlanCatalogAssignments(
      RE::FormID a_formID,
      std::vector<PlannedCatalogAssignment> &a_assignments) const;
  [[nodiscard]] int
  FindBestCatalogTargetRowIndex(const EquipmentWidgetItem &a_item,
                                bool a_requireAcceptable) const;
  void RebuildRowOrder();

  std::vector<VariantWorkbenchRow> rows_;
  std::vector<std::string> rowOrder_;
  std::unordered_map<std::string, std::string> activeDavVariants_;
  RE::FormID previewFormID_{0};
  std::unordered_map<std::string, std::string> previewDavVariants_;
};
} // namespace sosr::workbench
