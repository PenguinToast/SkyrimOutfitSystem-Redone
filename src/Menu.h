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
            Outfits
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
        void DrawWindow();
        void DrawGearTab();
        void DrawOutfitTab();

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
        ImGuiTextFilter gearSearch_;
        ImGuiTextFilter outfitSearch_;
    };
}
