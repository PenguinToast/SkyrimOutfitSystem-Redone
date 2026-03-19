#include "EquipmentCatalog.h"

#include <cstdio>
#include <iterator>

namespace
{
    using BipedSlot = RE::BIPED_MODEL::BipedObjectSlot;

    constexpr std::array<std::pair<BipedSlot, std::string_view>, 32> kArmorSlotNames{ {
        { BipedSlot::kHead, "30 - Head" },
        { BipedSlot::kHair, "31 - Hair" },
        { BipedSlot::kBody, "32 - Body" },
        { BipedSlot::kHands, "33 - Hands" },
        { BipedSlot::kForearms, "34 - Forearms" },
        { BipedSlot::kAmulet, "35 - Amulet" },
        { BipedSlot::kRing, "36 - Ring" },
        { BipedSlot::kFeet, "37 - Feet" },
        { BipedSlot::kCalves, "38 - Calves" },
        { BipedSlot::kShield, "39 - Shield" },
        { BipedSlot::kTail, "40 - Tail" },
        { BipedSlot::kLongHair, "41 - Long Hair" },
        { BipedSlot::kCirclet, "42 - Circlet" },
        { BipedSlot::kEars, "43 - Ears" },
        { BipedSlot::kModMouth, "44 - Mod Mouth" },
        { BipedSlot::kModNeck, "45 - Mod Neck" },
        { BipedSlot::kModChestPrimary, "46 - Mod Chest Primary" },
        { BipedSlot::kModBack, "47 - Mod Back" },
        { BipedSlot::kModMisc1, "48 - Mod Misc1" },
        { BipedSlot::kModPelvisPrimary, "49 - Mod Pelvis Primary" },
        { BipedSlot::kDecapitateHead, "50 - Decapitate Head" },
        { BipedSlot::kDecapitate, "51 - Decapitate" },
        { BipedSlot::kModPelvisSecondary, "52 - Mod Pelvis Secondary" },
        { BipedSlot::kModLegRight, "53 - Mod Leg Right" },
        { BipedSlot::kModLegLeft, "54 - Mod Leg Left" },
        { BipedSlot::kModFaceJewelry, "55 - Mod Face Jewelry" },
        { BipedSlot::kModChestSecondary, "56 - Mod Chest Secondary" },
        { BipedSlot::kModShoulder, "57 - Mod Shoulder" },
        { BipedSlot::kModArmLeft, "58 - Mod Arm Left" },
        { BipedSlot::kModArmRight, "59 - Mod Arm Right" },
        { BipedSlot::kModMisc2, "60 - Mod Misc2" },
        { BipedSlot::kFX01, "61 - FX01" }
    } };

    template <class Strings>
    std::string JoinStrings(const Strings& a_strings)
    {
        std::string output;
        for (const auto& value : a_strings) {
            if (!output.empty()) {
                output.append(", ");
            }
            output.append(value);
        }
        return output;
    }

    void SortUniqueStrings(std::vector<std::string>& a_values)
    {
        std::ranges::sort(a_values);
        a_values.erase(std::unique(a_values.begin(), a_values.end()), a_values.end());
    }

    std::string CopyCString(const char* a_text)
    {
        if (!a_text || a_text[0] == '\0') {
            return {};
        }
        return a_text;
    }

    std::string FormatFormID(RE::FormID a_formID)
    {
        char buffer[16]{};
        std::snprintf(buffer, sizeof(buffer), "%08X", a_formID);
        return buffer;
    }

    std::string GetPluginName(const RE::TESForm* a_form)
    {
        if (!a_form) {
            return "Unknown";
        }

        const auto* file = a_form->GetFile(0);
        if (!file) {
            return "Unknown";
        }

        const auto filename = file->GetFilename();
        if (filename.empty()) {
            return "Unknown";
        }

        return std::string(filename);
    }

    std::string GetEditorID(const RE::TESForm* a_form)
    {
        return a_form ? CopyCString(a_form->GetFormEditorID()) : std::string{};
    }

    std::string GetName(const RE::TESForm* a_form)
    {
        return a_form ? CopyCString(a_form->GetName()) : std::string{};
    }

    std::string GetDisplayName(const RE::TESForm* a_form)
    {
        if (!a_form) {
            return "Unknown";
        }

        auto name = GetName(a_form);
        if (!name.empty()) {
            return name;
        }

        auto editorID = GetEditorID(a_form);
        if (!editorID.empty()) {
            return editorID;
        }

        return "Form " + FormatFormID(a_form->GetFormID());
    }

    std::string BuildEntryID(const RE::TESForm* a_form)
    {
        if (!a_form) {
            return "unknown|00000000";
        }

        return GetPluginName(a_form) + "|" + FormatFormID(a_form->GetLocalFormID());
    }

    std::string GetArmorCategory(const RE::TESObjectARMO* a_armor)
    {
        if (!a_armor) {
            return "Armor";
        }

        switch (a_armor->GetArmorType()) {
        case RE::TESObjectARMO::ArmorType::kLightArmor:
            return "Light Armor";
        case RE::TESObjectARMO::ArmorType::kHeavyArmor:
            return "Heavy Armor";
        case RE::TESObjectARMO::ArmorType::kClothing:
            return "Clothing";
        default:
            return "Armor";
        }
    }

    std::vector<std::string> GetArmorSlots(const RE::TESObjectARMO* a_armor)
    {
        std::vector<std::string> slots;
        if (!a_armor) {
            return slots;
        }

        const auto slotMask = a_armor->GetSlotMask().underlying();
        for (const auto& [slot, label] : kArmorSlotNames) {
            if ((slotMask & static_cast<std::uint32_t>(std::to_underlying(slot))) != 0) {
                slots.emplace_back(label);
            }
        }

        if (slots.empty()) {
            slots.emplace_back("None");
        }

        return slots;
    }

    std::string GetPrimaryArmorSlot(const RE::TESObjectARMO* a_armor)
    {
        auto slots = GetArmorSlots(a_armor);
        return slots.empty() ? std::string{ "None" } : std::move(slots.front());
    }

    template <class T>
    std::vector<std::string> GetKeywords(const T* a_form)
    {
        std::vector<std::string> keywords;
        if (!a_form) {
            return keywords;
        }

        const auto* keywordForm = static_cast<const RE::BGSKeywordForm*>(a_form);
        keywords.reserve(keywordForm->GetNumKeywords());
        for (std::uint32_t index = 0; index < keywordForm->GetNumKeywords(); ++index) {
            const auto keyword = keywordForm->GetKeywordAt(index);
            if (!keyword || !keyword.value()) {
                continue;
            }

            auto text = GetEditorID(keyword.value());
            if (text.empty()) {
                text = GetName(keyword.value());
            }

            if (!text.empty()) {
                keywords.push_back(std::move(text));
            }
        }

        SortUniqueStrings(keywords);
        return keywords;
    }

    void AppendSearchToken(std::string& a_searchText, std::string_view a_token)
    {
        if (a_token.empty()) {
            return;
        }

        if (!a_searchText.empty()) {
            a_searchText.push_back(' ');
        }
        a_searchText.append(a_token);
    }

    std::string BuildGearSearchText(const sosng::GearEntry& a_entry)
    {
        std::string text;
        text.reserve(256);

        AppendSearchToken(text, a_entry.name);
        AppendSearchToken(text, a_entry.editorID);
        AppendSearchToken(text, a_entry.plugin);
        AppendSearchToken(text, a_entry.category);
        AppendSearchToken(text, a_entry.slot);
        AppendSearchToken(text, a_entry.keywordsText);

        return text;
    }

    std::string BuildOutfitSearchText(const sosng::OutfitEntry& a_entry)
    {
        std::string text;
        text.reserve(256);

        AppendSearchToken(text, a_entry.name);
        AppendSearchToken(text, a_entry.editorID);
        AppendSearchToken(text, a_entry.plugin);
        AppendSearchToken(text, a_entry.summary);
        AppendSearchToken(text, a_entry.tagsText);
        AppendSearchToken(text, a_entry.piecesText);

        return text;
    }

    struct OutfitDescription
    {
        std::vector<std::string> pieces;
        std::vector<std::string> tags;
        std::size_t armorCount{ 0 };
    };

    OutfitDescription DescribeOutfit(const RE::BGSOutfit* a_outfit)
    {
        OutfitDescription description;
        if (!a_outfit) {
            return description;
        }

        a_outfit->ForEachItem([&](RE::TESForm* a_item) {
            if (!a_item || a_item->IsDeleted() || a_item->IsIgnored()) {
                return RE::BSContainer::ForEachResult::kContinue;
            }

            description.pieces.push_back(GetDisplayName(a_item));

            if (const auto* armor = a_item->As<RE::TESObjectARMO>()) {
                ++description.armorCount;
                description.tags.emplace_back("Armor");
                description.tags.push_back(GetArmorCategory(armor));

                auto slots = GetArmorSlots(armor);
                description.tags.insert(description.tags.end(), std::make_move_iterator(slots.begin()), std::make_move_iterator(slots.end()));
            }

            return RE::BSContainer::ForEachResult::kContinue;
        });

        SortUniqueStrings(description.pieces);
        SortUniqueStrings(description.tags);
        return description;
    }

    std::string BuildOutfitSummary(const OutfitDescription& a_description)
    {
        const auto totalPieces = a_description.pieces.size();
        if (totalPieces == 0) {
            return "Empty outfit.";
        }

        std::string summary = "Contains " + std::to_string(totalPieces) + " item";
        if (totalPieces != 1) {
            summary.push_back('s');
        }

        if (a_description.armorCount > 0) {
            summary.append(": ");
            summary.append(std::to_string(a_description.armorCount));
            summary.append(" armor");
        } else {
            summary.push_back('.');
        }

        return summary;
    }

    template <class Entries, class Projection>
    std::vector<std::string> BuildSortedUniqueOptions(const Entries& a_entries, Projection a_projection)
    {
        std::vector<std::string> values;
        values.reserve(a_entries.size());

        for (const auto& entry : a_entries) {
            const auto& value = a_projection(entry);
            if (!value.empty()) {
                values.push_back(value);
            }
        }

        SortUniqueStrings(values);
        return values;
    }

    std::vector<std::string> BuildSortedUniqueOutfitTags(const std::vector<sosng::OutfitEntry>& a_outfits)
    {
        std::vector<std::string> values;
        for (const auto& outfit : a_outfits) {
            values.insert(values.end(), outfit.tags.begin(), outfit.tags.end());
        }

        SortUniqueStrings(values);
        return values;
    }
}

namespace sosng
{
    EquipmentCatalog& EquipmentCatalog::Get()
    {
        static EquipmentCatalog singleton;
        return singleton;
    }

    EquipmentCatalog::EquipmentCatalog() :
        source_("Catalog not loaded"),
        revision_("cache-empty")
    {}

    void EquipmentCatalog::RefreshFromGame()
    {
        gear_.clear();
        outfits_.clear();
        gearPlugins_.clear();
        gearSlots_.clear();
        outfitPlugins_.clear();
        outfitTags_.clear();

        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) {
            source_ = "TESDataHandler unavailable";
            revision_ = "cache-error";
            logger::error("Failed to refresh equipment catalog: TESDataHandler was null");
            return;
        }

        for (auto* armor : dataHandler->GetFormArray<RE::TESObjectARMO>()) {
            if (!armor || armor->IsDeleted() || armor->IsIgnored() || !armor->GetFile(0)) {
                continue;
            }

            const auto editorID = GetEditorID(armor);
            auto displayName = GetName(armor);
            if (displayName.empty()) {
                displayName = editorID;
            }
            if (displayName.empty()) {
                continue;
            }

            GearEntry entry{};
            entry.formID = armor->GetFormID();
            entry.id = BuildEntryID(armor);
            entry.name = std::move(displayName);
            entry.editorID = editorID;
            entry.plugin = GetPluginName(armor);
            entry.category = GetArmorCategory(armor);
            entry.slot = GetPrimaryArmorSlot(armor);
            entry.statValue = static_cast<int>(armor->GetArmorRating());
            entry.weight = armor->GetWeight();
            entry.value = armor->GetGoldValue();
            entry.keywords = GetKeywords(armor);

            auto slots = GetArmorSlots(armor);
            entry.keywords.insert(entry.keywords.end(), std::make_move_iterator(slots.begin()), std::make_move_iterator(slots.end()));
            entry.keywords.push_back(entry.category);
            SortUniqueStrings(entry.keywords);
            entry.keywordsText = JoinStrings(entry.keywords);
            entry.searchText = BuildGearSearchText(entry);

            gear_.push_back(std::move(entry));
        }
        for (auto* outfit : dataHandler->GetFormArray<RE::BGSOutfit>()) {
            if (!outfit || outfit->IsDeleted() || outfit->IsIgnored() || !outfit->GetFile(0)) {
                continue;
            }

            auto description = DescribeOutfit(outfit);
            if (description.pieces.empty()) {
                continue;
            }

            auto editorID = GetEditorID(outfit);
            auto displayName = GetName(outfit);
            if (displayName.empty()) {
                displayName = editorID;
            }
            if (displayName.empty()) {
                displayName = "Form " + FormatFormID(outfit->GetFormID());
            }

            OutfitEntry entry{};
            entry.formID = outfit->GetFormID();
            entry.id = BuildEntryID(outfit);
            entry.name = std::move(displayName);
            entry.editorID = std::move(editorID);
            entry.plugin = GetPluginName(outfit);
            entry.summary = BuildOutfitSummary(description);
            entry.pieces = std::move(description.pieces);
            entry.tags = std::move(description.tags);

            entry.piecesText = JoinStrings(entry.pieces);
            entry.tagsText = JoinStrings(entry.tags);
            entry.searchText = BuildOutfitSearchText(entry);

            outfits_.push_back(std::move(entry));
        }

        RebuildDerivedData();

        source_ = "Runtime cache from TESDataHandler";
        revision_ = "armor=" + std::to_string(gear_.size()) + ", outfits=" + std::to_string(outfits_.size());

        logger::info("Equipment catalog refreshed: {} gear entries, {} outfits", gear_.size(), outfits_.size());
    }

    void EquipmentCatalog::RebuildDerivedData()
    {
        gearPlugins_ = BuildSortedUniqueOptions(gear_, [](const GearEntry& a_entry) -> const std::string& {
            return a_entry.plugin;
        });
        gearSlots_ = BuildSortedUniqueOptions(gear_, [](const GearEntry& a_entry) -> const std::string& {
            return a_entry.slot;
        });
        outfitPlugins_ = BuildSortedUniqueOptions(outfits_, [](const OutfitEntry& a_entry) -> const std::string& {
            return a_entry.plugin;
        });
        outfitTags_ = BuildSortedUniqueOutfitTags(outfits_);
    }
}
