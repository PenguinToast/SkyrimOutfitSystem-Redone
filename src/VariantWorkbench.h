#pragma once

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <string>
#include <unordered_map>
#include <vector>

namespace sosng::workbench {
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
  [[nodiscard]] bool CanAcceptOverride(int a_targetRowIndex,
                                       const EquipmentWidgetItem &a_item,
                                       int a_sourceRowIndex = -1,
                                       int a_sourceItemIndex = -1) const;
  bool AddCatalogOverride(int a_targetRowIndex, RE::FormID a_formID);
  bool MoveOverride(int a_sourceRowIndex, int a_sourceItemIndex,
                    int a_targetRowIndex);
  bool DeleteOverride(int a_rowIndex, int a_itemIndex);
  bool DeleteRow(int a_rowIndex);
  bool SetHideEquipped(int a_rowIndex, bool a_hideEquipped);
  bool InsertCatalogRow(RE::FormID a_formID, int a_targetRowIndex,
                        bool a_insertAfter);
  bool ApplyRowReorder(int a_sourceRowIndex, int a_targetRowIndex,
                       bool a_insertAfter);
  void SyncDynamicArmorVariants();
  void Serialize(SKSE::SerializationInterface *a_skse) const;
  void Deserialize(SKSE::SerializationInterface *a_skse);
  void Revert();

  [[nodiscard]] const std::vector<VariantWorkbenchRow> &GetRows() const {
    return rows_;
  }
  [[nodiscard]] std::size_t GetRowCount() const { return rows_.size(); }

private:
  std::vector<VariantWorkbenchRow> rows_;
  std::vector<std::string> rowOrder_;
  std::unordered_map<std::string, std::string> activeDavVariants_;
};
} // namespace sosng::workbench
