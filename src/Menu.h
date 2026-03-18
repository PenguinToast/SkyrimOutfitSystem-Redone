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
        enum class BrowserTab
        {
            Gear,
            Outfits,
            Options
        };

        enum class GearKindFilter
        {
            All,
            Armor,
            Weapon
        };

        static constexpr std::array<std::string_view, 3> kGearKindLabels{ "All gear", "Armors", "Weapons" };

        Menu() = default;

        void ApplyStyle();
        void LoadUserSettings();
        void SaveUserSettings() const;
        void RebuildFontAtlas();
        void DrawWindow();
        void DrawGearTab();
        void DrawOutfitTab();
        void DrawOptionsTab();
        bool DrawSearchableStringCombo(const char* a_label, const char* a_allLabel, const std::vector<std::string>& a_options, int& a_index, ImGuiTextFilter& a_filter);

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
        GearKindFilter gearKindFilter_{ GearKindFilter::All };
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
    };
}
