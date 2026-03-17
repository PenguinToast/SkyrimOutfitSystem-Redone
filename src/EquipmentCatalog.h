#pragma once

namespace sosng
{
    enum class GearKind
    {
        Armor,
        Weapon
    };

    struct GearEntry
    {
        std::string_view id;
        std::string_view name;
        std::string_view editorID;
        std::string_view plugin;
        GearKind kind;
        std::string_view category;
        std::string_view slot;
        int statValue;
        float weight;
        int value;
        std::vector<std::string_view> keywords;
    };

    struct OutfitEntry
    {
        std::string_view id;
        std::string_view name;
        std::string_view editorID;
        std::string_view plugin;
        std::string_view summary;
        std::vector<std::string_view> pieces;
        std::vector<std::string_view> tags;
    };

    class EquipmentCatalog
    {
    public:
        static const EquipmentCatalog& Get();

        const std::vector<GearEntry>& GetGear() const { return gear_; }
        const std::vector<OutfitEntry>& GetOutfits() const { return outfits_; }

        const std::vector<std::string_view>& GetGearPlugins() const { return gearPlugins_; }
        const std::vector<std::string_view>& GetGearSlots() const { return gearSlots_; }
        const std::vector<std::string_view>& GetOutfitPlugins() const { return outfitPlugins_; }
        const std::vector<std::string_view>& GetOutfitTags() const { return outfitTags_; }

        std::string_view GetSource() const { return source_; }
        std::string_view GetRevision() const { return revision_; }

    private:
        EquipmentCatalog();

        std::vector<GearEntry> gear_;
        std::vector<OutfitEntry> outfits_;
        std::vector<std::string_view> gearPlugins_;
        std::vector<std::string_view> gearSlots_;
        std::vector<std::string_view> outfitPlugins_;
        std::vector<std::string_view> outfitTags_;
        std::string_view source_;
        std::string_view revision_;
    };
}
