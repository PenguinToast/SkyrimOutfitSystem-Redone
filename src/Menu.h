#pragma once

#include "EquipmentCatalog.h"
#include "imgui.h"

struct IDXGISwapChain;
struct ID3D11Device;
struct ID3D11DeviceContext;

namespace sosng
{
    class Menu
    {
    public:
        static Menu* GetSingleton();

        void Init(IDXGISwapChain* a_swapChain, ID3D11Device* a_device, ID3D11DeviceContext* a_context);
        void Draw();
        void Open();
        void Close();
        void Toggle();
        bool IsEnabled() const { return enabled_; }
        bool IsInitialized() const { return initialized_; }

    private:
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

        enum class DragSourceKind : std::uint32_t
        {
            Catalog = 1,
            Override = 2,
            Row = 3
        };

        struct DraggedEquipmentPayload
        {
            std::uint32_t sourceKind{ 0 };
            std::int32_t rowIndex{ -1 };
            std::int32_t itemIndex{ -1 };
            RE::FormID formID{ 0 };
        };

        enum class BrowserTab
        {
            Gear,
            Outfits,
            Options
        };

        Menu() = default;

        void ApplyStyle();
        void LoadUserSettings();
        void SaveUserSettings() const;
        void RebuildFontAtlas();
        void DrawWindow();
        void DrawGearTab();
        void DrawGearCatalogTable(const std::vector<const GearEntry*>& a_rows);
        void DrawVariantWorkbenchPane();
        void DrawOutfitTab();
        void DrawOptionsTab();
        bool DrawSearchableStringCombo(const char* a_label, const char* a_allLabel, const std::vector<std::string>& a_options, int& a_index, ImGuiTextFilter& a_filter);
        bool DrawEquipmentInfoWidget(const char* a_id, const EquipmentWidgetItem& a_item, bool a_allowDrag, DragSourceKind a_sourceKind, bool a_showDeleteButton = false, int a_rowIndex = -1, int a_itemIndex = -1);
        void SyncVariantRowsFromPlayer();
        bool BuildCatalogItem(RE::FormID a_formID, EquipmentWidgetItem& a_item) const;
        bool CanAcceptOverride(int a_targetRowIndex, const EquipmentWidgetItem& a_item, int a_sourceRowIndex = -1, int a_sourceItemIndex = -1) const;
        void AcceptOverridePayload(int a_targetRowIndex);
        void ApplyRowReorder(const DraggedEquipmentPayload& a_dragPayload, int a_targetRowIndex, bool a_insertAfter);
        void AcceptOverrideDeletePayload();

        bool MatchesGearFilters(const GearEntry& a_entry) const;
        bool MatchesOutfitFilters(const OutfitEntry& a_entry) const;

        std::vector<const GearEntry*> BuildFilteredGear() const;
        std::vector<const OutfitEntry*> BuildFilteredOutfits() const;
        void SortGearRows(std::vector<const GearEntry*>& a_rows, ImGuiTableSortSpecs* a_sortSpecs) const;
        void SortOutfitRows(std::vector<const OutfitEntry*>& a_rows, ImGuiTableSortSpecs* a_sortSpecs) const;

        bool initialized_{ false };
        bool enabled_{ false };
        ID3D11Device* device_{ nullptr };
        ID3D11DeviceContext* context_{ nullptr };
        BrowserTab activeTab_{ BrowserTab::Gear };
        int gearPluginIndex_{ 0 };
        int gearSlotIndex_{ 0 };
        int outfitPluginIndex_{ 0 };
        int outfitTagIndex_{ 0 };
        int fontSizePixels_{ 13 };
        int pendingFontSizePixels_{ 13 };
        bool pendingFontAtlasRebuild_{ false };
        std::string settingsDirectory_;
        std::string imguiIniPath_;
        std::string userSettingsPath_;
        ImGuiTextFilter gearSearch_;
        ImGuiTextFilter outfitSearch_;
        ImGuiTextFilter gearPluginFilter_;
        ImGuiTextFilter outfitPluginFilter_;
        std::vector<VariantWorkbenchRow> variantRows_;
        std::vector<std::string> variantRowOrder_;
    };
}
