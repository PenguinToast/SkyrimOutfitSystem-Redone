#include "EquipmentCatalog.h"

namespace
{
    template <class Entries, class Projection>
    std::vector<std::string_view> BuildSortedUniqueOptions(const Entries& a_entries, Projection a_projection)
    {
        std::vector<std::string_view> values;
        values.reserve(a_entries.size());

        for (const auto& entry : a_entries) {
            values.push_back(a_projection(entry));
        }

        std::ranges::sort(values);
        values.erase(std::unique(values.begin(), values.end()), values.end());
        return values;
    }

    std::vector<std::string_view> BuildSortedUniqueOutfitTags(const std::vector<sosng::OutfitEntry>& a_outfits)
    {
        std::vector<std::string_view> values;
        for (const auto& outfit : a_outfits) {
            values.insert(values.end(), outfit.tags.begin(), outfit.tags.end());
        }

        std::ranges::sort(values);
        values.erase(std::unique(values.begin(), values.end()), values.end());
        return values;
    }
}

namespace sosng
{
    const EquipmentCatalog& EquipmentCatalog::Get()
    {
        static const EquipmentCatalog singleton;
        return singleton;
    }

    EquipmentCatalog::EquipmentCatalog() :
        source_("Bundled preview catalog"),
        revision_("imgui-prototype")
    {
        gear_ = {
            { "armor-ebonymail", "Ebony Mail", "DA01EbonyMail", "Skyrim.esm", GearKind::Armor, "Heavy Armor", "Body", 45, 28.0f, 3200, { "daedric artifact", "body", "heavy", "stealth" } },
            { "armor-nightingale", "Nightingale Armor", "NightingaleArmorCuirass", "Skyrim.esm", GearKind::Armor, "Light Armor", "Body", 34, 12.0f, 2100, { "thieves guild", "body", "light", "stealth" } },
            { "armor-travelcloak", "Traveler Fur Mantle", "SOSNGTravelerMantle01", "WarmFashion.esp", GearKind::Armor, "Clothing", "Cloak", 0, 2.0f, 240, { "cloak", "travel", "fur", "vanity" } },
            { "armor-glassgauntlets", "Glass Gauntlets", "ArmorGlassGauntlets", "Skyrim.esm", GearKind::Armor, "Light Armor", "Hands", 11, 4.0f, 450, { "hands", "light", "glass" } },
            { "armor-bosmerboots", "Bosmer Hunter Boots", "SOSNGBosmerHunterBoots", "BosmeriSwag.esp", GearKind::Armor, "Light Armor", "Feet", 9, 3.0f, 380, { "boots", "hunter", "bosmer", "feet" } },
            { "armor-courtcirclet", "Silver Court Circlet", "SOSNGCourtCircletSilver", "RegalThreads.esp", GearKind::Armor, "Clothing", "Head", 0, 1.0f, 900, { "circlet", "head", "noble", "social" } },
            { "weapon-dawnbreaker", "Dawnbreaker", "DA09Dawnbreaker", "Skyrim.esm", GearKind::Weapon, "Sword", "One-Handed", 12, 10.0f, 740, { "sword", "one-handed", "artifact", "fire" } },
            { "weapon-dragonbonebow", "Dragonbone Bow", "DLC2DragonBoneBow", "Dawnguard.esm", GearKind::Weapon, "Bow", "Two-Handed", 20, 20.0f, 2725, { "bow", "dragonbone", "archery" } },
            { "weapon-orsimerdagger", "Orsimer Skinning Knife", "SOSNGOrsimerKnife01", "OrcStrongholds.esp", GearKind::Weapon, "Dagger", "One-Handed", 8, 2.0f, 180, { "dagger", "knife", "orc", "utility" } },
            { "weapon-ritualstaff", "Ritual Bone Staff", "SOSNGRitualBoneStaff", "Witchlight.esp", GearKind::Weapon, "Staff", "Two-Handed", 0, 8.0f, 1250, { "staff", "ritual", "bone", "magic" } },
            { "weapon-steelwaraxe", "Steel War Axe", "WeaponSteelWarAxe", "Skyrim.esm", GearKind::Weapon, "War Axe", "One-Handed", 9, 11.0f, 55, { "axe", "one-handed", "steel" } },
            { "weapon-nordicherosword", "Nordic Hero Greatsword", "DLC2NordicHeroGreatsword", "Dragonborn.esm", GearKind::Weapon, "Greatsword", "Two-Handed", 21, 16.0f, 985, { "greatsword", "nordic", "two-handed" } }
        };

        outfits_ = {
            { "outfit-roadrunner", "Roadrunner Kit", "SOSNGOutfitRoadrunner", "SkyrimOutfitSystemNG.esp", "Lightweight travel set for cities, inns, and carriage routes.", { "Traveler Fur Mantle", "Bosmer Hunter Boots", "Silver Court Circlet" }, { "travel", "light", "city" } },
            { "outfit-nightops", "Night Ops", "SOSNGOutfitNightOps", "SkyrimOutfitSystemNG.esp", "Stealth-forward vanity kit built around dark leather silhouettes.", { "Nightingale Armor", "Glass Gauntlets", "Orsimer Skinning Knife" }, { "stealth", "thief", "light armor" } },
            { "outfit-courtly", "Courtly Presence", "SOSNGOutfitCourtlyPresence", "SkyrimOutfitSystemNG.esp", "Social outfit for dialogue scenes, guild meetings, and palace visits.", { "Silver Court Circlet", "Traveler Fur Mantle" }, { "social", "noble", "city" } },
            { "outfit-dwemerbreaker", "Dwemer Breaker", "SOSNGOutfitDwemerBreaker", "SkyrimOutfitSystemNG.esp", "Heavy ruin-delving silhouette with practical gloves and a visible artifact chestpiece.", { "Ebony Mail", "Glass Gauntlets", "Steel War Axe" }, { "dungeon", "heavy", "dwemer" } },
            { "outfit-dragonguard", "Dragonguard Hunter", "SOSNGOutfitDragonguardHunter", "SkyrimOutfitSystemNG.esp", "Ranged-focused look with a dragonbone bow and traveler-layered profile.", { "Dragonbone Bow", "Traveler Fur Mantle", "Bosmer Hunter Boots" }, { "archery", "hunter", "field" } }
        };

        gearPlugins_ = BuildSortedUniqueOptions(gear_, [](const GearEntry& a_entry) { return a_entry.plugin; });
        gearSlots_ = BuildSortedUniqueOptions(gear_, [](const GearEntry& a_entry) { return a_entry.slot; });
        outfitPlugins_ = BuildSortedUniqueOptions(outfits_, [](const OutfitEntry& a_entry) { return a_entry.plugin; });
        outfitTags_ = BuildSortedUniqueOutfitTags(outfits_);
    }
}
