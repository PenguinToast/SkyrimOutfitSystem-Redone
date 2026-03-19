#pragma once

#include <RE/Skyrim.h>

namespace sosng::workbench
{
    struct EquipmentWidgetItem
    {
        RE::FormID formID{ 0 };
        std::string key;
        std::string name;
        std::string slotText;
        std::uint64_t slotMask{ 0 };
    };

    struct VariantWorkbenchRow
    {
        std::string key;
        EquipmentWidgetItem equipped;
        std::vector<EquipmentWidgetItem> overrides;
    };

    class VariantWorkbench
    {
    public:
        void SyncRowsFromPlayer();
        bool BuildCatalogItem(RE::FormID a_formID, EquipmentWidgetItem& a_item) const;
        bool CanAcceptOverride(int a_targetRowIndex, const EquipmentWidgetItem& a_item, int a_sourceRowIndex = -1, int a_sourceItemIndex = -1) const;
        bool AddCatalogOverride(int a_targetRowIndex, RE::FormID a_formID);
        bool MoveOverride(int a_sourceRowIndex, int a_sourceItemIndex, int a_targetRowIndex);
        bool DeleteOverride(int a_rowIndex, int a_itemIndex);
        bool ApplyRowReorder(int a_sourceRowIndex, int a_targetRowIndex, bool a_insertAfter);
        void SyncDynamicArmorVariants();

        const std::vector<VariantWorkbenchRow>& GetRows() const { return rows_; }
        std::size_t GetRowCount() const { return rows_.size(); }

    private:
        std::vector<VariantWorkbenchRow> rows_;
        std::vector<std::string> rowOrder_;
        std::unordered_map<std::string, std::string> activeDavVariants_;
    };
}
