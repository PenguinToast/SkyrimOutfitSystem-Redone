#include "catalog/EntryBuilders.h"

#include "ArmorUtils.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <unordered_set>

namespace {
using BipedSlot = RE::BIPED_MODEL::BipedObjectSlot;

const auto kModexKitPath =
    std::filesystem::path("data") / "interface" / "modex" / "user" / "kits";

constexpr std::array<std::pair<BipedSlot, std::string_view>, 32>
    kArmorSlotNames{
        {{BipedSlot::kHead, "30 - Head"},
         {BipedSlot::kHair, "31 - Hair"},
         {BipedSlot::kBody, "32 - Body"},
         {BipedSlot::kHands, "33 - Hands"},
         {BipedSlot::kForearms, "34 - Forearms"},
         {BipedSlot::kAmulet, "35 - Amulet"},
         {BipedSlot::kRing, "36 - Ring"},
         {BipedSlot::kFeet, "37 - Feet"},
         {BipedSlot::kCalves, "38 - Calves"},
         {BipedSlot::kShield, "39 - Shield"},
         {BipedSlot::kTail, "40 - Tail"},
         {BipedSlot::kLongHair, "41 - Long Hair"},
         {BipedSlot::kCirclet, "42 - Circlet"},
         {BipedSlot::kEars, "43 - Ears"},
         {BipedSlot::kModMouth, "44 - Mod Mouth"},
         {BipedSlot::kModNeck, "45 - Mod Neck"},
         {BipedSlot::kModChestPrimary, "46 - Mod Chest Primary"},
         {BipedSlot::kModBack, "47 - Mod Back"},
         {BipedSlot::kModMisc1, "48 - Mod Misc1"},
         {BipedSlot::kModPelvisPrimary, "49 - Mod Pelvis Primary"},
         {BipedSlot::kDecapitateHead, "50 - Decapitate Head"},
         {BipedSlot::kDecapitate, "51 - Decapitate"},
         {BipedSlot::kModPelvisSecondary, "52 - Mod Pelvis Secondary"},
         {BipedSlot::kModLegRight, "53 - Mod Leg Right"},
         {BipedSlot::kModLegLeft, "54 - Mod Leg Left"},
         {BipedSlot::kModFaceJewelry, "55 - Mod Face Jewelry"},
         {BipedSlot::kModChestSecondary, "56 - Mod Chest Secondary"},
         {BipedSlot::kModShoulder, "57 - Mod Shoulder"},
         {BipedSlot::kModArmLeft, "58 - Mod Arm Left"},
         {BipedSlot::kModArmRight, "59 - Mod Arm Right"},
         {BipedSlot::kModMisc2, "60 - Mod Misc2"},
         {BipedSlot::kFX01, "61 - FX01"}}};

template <class Strings> std::string JoinStrings(const Strings &a_strings) {
  std::string output;
  for (const auto &value : a_strings) {
    if (!output.empty()) {
      output.append(", ");
    }
    output.append(value);
  }
  return output;
}

void SortUniqueStrings(std::vector<std::string> &a_values) {
  std::ranges::sort(a_values);
  a_values.erase(std::unique(a_values.begin(), a_values.end()), a_values.end());
}

std::string CopyCString(const char *a_text) {
  if (!a_text || a_text[0] == '\0') {
    return {};
  }
  return a_text;
}

std::string GetName(const RE::TESForm *a_form) {
  return a_form ? CopyCString(a_form->GetName()) : std::string{};
}

std::string BuildEntryID(const RE::TESForm *a_form) {
  if (!a_form) {
    return "unknown|00000000";
  }

  const auto plugin = sosr::armor::GetPluginName(a_form);
  return (plugin.empty() ? std::string{"Unknown"} : plugin) + "|" +
         sosr::armor::FormatFormID(a_form->GetLocalFormID());
}

std::string GetArmorCategory(const RE::TESObjectARMO *a_armor) {
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

std::vector<std::string> GetArmorSlots(const RE::TESObjectARMO *a_armor) {
  std::vector<std::string> slots;
  if (!a_armor) {
    return slots;
  }

  const auto slotMask = a_armor->GetSlotMask().underlying();
  for (const auto &[slot, label] : kArmorSlotNames) {
    if ((slotMask & static_cast<std::uint32_t>(std::to_underlying(slot))) !=
        0) {
      slots.emplace_back(label);
    }
  }

  if (slots.empty()) {
    slots.emplace_back("None");
  }

  return slots;
}

std::string GetPrimaryArmorSlot(const RE::TESObjectARMO *a_armor) {
  auto slots = GetArmorSlots(a_armor);
  return slots.empty() ? std::string{"None"} : std::move(slots.front());
}

sosr::CatalogCollectionItemNode BuildCachedCollectionNode(
    const RE::TESForm *a_form, const std::int32_t a_level,
    std::vector<sosr::CatalogCollectionItemNode> a_children = {}) {
  sosr::CatalogCollectionItemNode node{};
  node.formID = a_form ? a_form->GetFormID() : 0;
  node.level = a_level;
  node.children = std::move(a_children);

  if (!a_form || a_form->As<RE::TESObjectARMO>()) {
    return node;
  }

  node.cachedName = sosr::armor::GetDisplayName(a_form);
  return node;
}

template <class T> std::vector<std::string> GetKeywords(const T *a_form) {
  std::vector<std::string> keywords;
  if (!a_form) {
    return keywords;
  }

  const auto *keywordForm = static_cast<const RE::BGSKeywordForm *>(a_form);
  keywords.reserve(keywordForm->GetNumKeywords());
  for (std::uint32_t index = 0; index < keywordForm->GetNumKeywords();
       ++index) {
    const auto keyword = keywordForm->GetKeywordAt(index);
    if (!keyword || !keyword.value()) {
      continue;
    }

    auto text = sosr::armor::GetEditorID(keyword.value());
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

void AppendSearchToken(std::string &a_searchText, std::string_view a_token) {
  if (a_token.empty()) {
    return;
  }

  if (!a_searchText.empty()) {
    a_searchText.push_back(' ');
  }
  a_searchText.append(a_token);
}

std::string BuildGearSearchText(const sosr::GearEntry &a_entry) {
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

std::string BuildOutfitSearchText(const sosr::OutfitEntry &a_entry) {
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

std::string BuildKitSearchText(const sosr::KitEntry &a_entry) {
  std::string text;
  text.reserve(256);

  AppendSearchToken(text, a_entry.name);
  AppendSearchToken(text, a_entry.collection);
  AppendSearchToken(text, a_entry.summary);
  AppendSearchToken(text, a_entry.piecesText);
  AppendSearchToken(text, a_entry.slotsText);

  return text;
}

struct OutfitDescription {
  std::vector<RE::FormID> armorFormIDs;
  std::vector<sosr::CatalogCollectionItemNode> itemTree;
  std::vector<std::string> pieces;
  std::vector<std::string> slots;
  std::vector<std::string> tags;
  std::size_t armorCount{0};
};

struct KitDescription {
  std::vector<RE::FormID> armorFormIDs;
  std::vector<sosr::CatalogCollectionItemNode> itemTree;
  std::vector<std::string> pieces;
  std::vector<std::string> slots;
  bool hasMissingItems{false};
};

void AppendCachedLeveledListDescription(
    const sosr::ResolvedReferenceCollection &a_cache,
    OutfitDescription &a_description,
    std::unordered_set<RE::FormID> &a_seenArmor) {
  a_description.pieces.insert(a_description.pieces.end(),
                              a_cache.pieces.begin(), a_cache.pieces.end());
  a_description.slots.insert(a_description.slots.end(), a_cache.slots.begin(),
                             a_cache.slots.end());
  a_description.tags.insert(a_description.tags.end(), a_cache.tags.begin(),
                            a_cache.tags.end());
  for (const auto formID : a_cache.armorFormIDs) {
    if (a_seenArmor.insert(formID).second) {
      a_description.armorFormIDs.push_back(formID);
      ++a_description.armorCount;
    }
  }
}

auto GetOrBuildLeveledListCache(
    const RE::FormID a_formID, const RE::TESLeveledList *a_list,
    std::unordered_map<RE::FormID, sosr::ResolvedReferenceCollection> &a_cache,
    std::unordered_set<RE::FormID> &a_activeLeveledLists)
    -> const sosr::ResolvedReferenceCollection & {
  const auto cacheIt = a_cache.find(a_formID);
  if (cacheIt != a_cache.end()) {
    return cacheIt->second;
  }

  sosr::ResolvedReferenceCollection built;
  if (!a_activeLeveledLists.insert(a_formID).second) {
    return a_cache.emplace(a_formID, std::move(built)).first->second;
  }

  std::unordered_set<RE::FormID> seenArmorForms;
  std::unordered_set<RE::FormID> seenDirectArmorChildren;
  for (const auto &entry : a_list->entries) {
    const auto *form = entry.form;
    if (!form || form->IsDeleted() || form->IsIgnored()) {
      continue;
    }

    if (const auto *armor = form->As<RE::TESObjectARMO>()) {
      if (seenDirectArmorChildren.insert(armor->GetFormID()).second) {
        built.itemTree.push_back(
            {.formID = armor->GetFormID(), .level = entry.level});
      }
      built.pieces.push_back(sosr::armor::GetDisplayName(armor));

      if (seenArmorForms.insert(armor->GetFormID()).second) {
        built.armorFormIDs.push_back(armor->GetFormID());
        built.tags.emplace_back("Armor");
        built.tags.push_back(GetArmorCategory(armor));

        auto slots = GetArmorSlots(armor);
        built.slots.insert(built.slots.end(), slots.begin(), slots.end());
        built.tags.insert(built.tags.end(),
                          std::make_move_iterator(slots.begin()),
                          std::make_move_iterator(slots.end()));
      }
      continue;
    }

    if (form->GetFormType() == RE::FormType::LeveledItem) {
      const auto *nestedList = form->As<RE::TESLeveledList>();
      if (!nestedList) {
        continue;
      }

      const auto &nestedCache = GetOrBuildLeveledListCache(
          form->GetFormID(), nestedList, a_cache, a_activeLeveledLists);
      built.itemTree.push_back(
          BuildCachedCollectionNode(form, entry.level, nestedCache.itemTree));
      built.pieces.insert(built.pieces.end(), nestedCache.pieces.begin(),
                          nestedCache.pieces.end());
      built.slots.insert(built.slots.end(), nestedCache.slots.begin(),
                         nestedCache.slots.end());
      built.tags.insert(built.tags.end(), nestedCache.tags.begin(),
                        nestedCache.tags.end());
      for (const auto formID : nestedCache.armorFormIDs) {
        if (seenArmorForms.insert(formID).second) {
          built.armorFormIDs.push_back(formID);
        }
      }
      continue;
    }

    built.itemTree.push_back(BuildCachedCollectionNode(form, entry.level));
    built.pieces.push_back(sosr::armor::GetDisplayName(form));
  }

  a_activeLeveledLists.erase(a_formID);
  SortUniqueStrings(built.pieces);
  SortUniqueStrings(built.slots);
  SortUniqueStrings(built.tags);
  return a_cache.emplace(a_formID, std::move(built)).first->second;
}

void AccumulateArmorDescription(const RE::TESObjectARMO *a_armor,
                                OutfitDescription &a_description,
                                std::unordered_set<RE::FormID> &a_seenArmor) {
  if (!a_armor) {
    return;
  }

  a_description.pieces.push_back(sosr::armor::GetDisplayName(a_armor));

  if (!a_seenArmor.insert(a_armor->GetFormID()).second) {
    return;
  }

  a_description.armorFormIDs.push_back(a_armor->GetFormID());
  ++a_description.armorCount;
  a_description.tags.emplace_back("Armor");
  a_description.tags.push_back(GetArmorCategory(a_armor));

  auto slots = GetArmorSlots(a_armor);
  a_description.slots.insert(a_description.slots.end(), slots.begin(),
                             slots.end());
  a_description.tags.insert(a_description.tags.end(),
                            std::make_move_iterator(slots.begin()),
                            std::make_move_iterator(slots.end()));
}

auto BuildOutfitItemNode(
    const RE::TESForm *a_item, OutfitDescription &a_description,
    std::unordered_set<RE::FormID> &a_seenArmor,
    std::unordered_set<RE::FormID> &a_activeLeveledLists,
    std::unordered_map<RE::FormID, sosr::ResolvedReferenceCollection>
        &a_leveledListCache) -> std::optional<sosr::CatalogCollectionItemNode> {
  if (!a_item || a_item->IsDeleted() || a_item->IsIgnored()) {
    return std::nullopt;
  }

  if (const auto *armor = a_item->As<RE::TESObjectARMO>()) {
    AccumulateArmorDescription(armor, a_description, a_seenArmor);
    return sosr::CatalogCollectionItemNode{.formID = armor->GetFormID()};
  }

  if (a_item->GetFormType() == RE::FormType::LeveledItem) {
    const auto *list = a_item->As<RE::TESLeveledList>();
    if (!list) {
      return std::nullopt;
    }

    const auto &cache = GetOrBuildLeveledListCache(
        a_item->GetFormID(), list, a_leveledListCache, a_activeLeveledLists);
    AppendCachedLeveledListDescription(cache, a_description, a_seenArmor);
    return BuildCachedCollectionNode(a_item, -1, cache.itemTree);
  }

  a_description.pieces.push_back(sosr::armor::GetDisplayName(a_item));
  return BuildCachedCollectionNode(a_item, -1);
}

OutfitDescription
DescribeOutfit(const RE::BGSOutfit *a_outfit,
               std::unordered_map<RE::FormID, sosr::ResolvedReferenceCollection>
                   &a_leveledListCache) {
  OutfitDescription description;
  if (!a_outfit) {
    return description;
  }

  std::unordered_set<RE::FormID> seenArmorForms;
  std::unordered_set<RE::FormID> activeLeveledLists;

  a_outfit->ForEachItem([&](RE::TESForm *a_item) {
    if (const auto node =
            BuildOutfitItemNode(a_item, description, seenArmorForms,
                                activeLeveledLists, a_leveledListCache)) {
      description.itemTree.push_back(*node);
    }

    return RE::BSContainer::ForEachResult::kContinue;
  });

  SortUniqueStrings(description.pieces);
  SortUniqueStrings(description.slots);
  SortUniqueStrings(description.tags);
  return description;
}

std::string BuildOutfitSummary(const OutfitDescription &a_description) {
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

nlohmann::json OpenJsonFile(const std::filesystem::path &a_path) {
  if (!std::filesystem::exists(a_path)) {
    return nlohmann::json::object();
  }

  try {
    std::ifstream file(a_path);
    if (!file.is_open()) {
      return nlohmann::json::object();
    }

    nlohmann::json data;
    file >> data;
    return data;
  } catch (...) {
    return nlohmann::json::object();
  }
}

KitDescription DescribeKitItems(const nlohmann::json &a_items) {
  KitDescription description;
  if (!a_items.is_object()) {
    return description;
  }

  std::unordered_set<RE::FormID> seenArmorForms;
  for (const auto &[editorID, _] : a_items.items()) {
    auto *form = RE::TESForm::LookupByEditorID(editorID);
    if (!form) {
      description.hasMissingItems = true;
      break;
    }

    const auto *armor = form->As<RE::TESObjectARMO>();
    if (!armor) {
      continue;
    }

    if (!seenArmorForms.insert(armor->GetFormID()).second) {
      continue;
    }

    description.armorFormIDs.push_back(armor->GetFormID());
    description.pieces.push_back(sosr::armor::GetDisplayName(armor));
    description.itemTree.push_back({.formID = armor->GetFormID()});

    auto slots = GetArmorSlots(armor);
    description.slots.insert(description.slots.end(), slots.begin(),
                             slots.end());
  }

  SortUniqueStrings(description.pieces);
  SortUniqueStrings(description.slots);
  return description;
}

std::string BuildKitSummary(const KitDescription &a_description) {
  const auto armorCount = a_description.armorFormIDs.size();
  if (armorCount == 0) {
    return "No armor items.";
  }

  std::string summary =
      "Contains " + std::to_string(armorCount) + " armor item";
  if (armorCount != 1) {
    summary.push_back('s');
  }
  return summary;
}
} // namespace

namespace sosr::catalog {
const std::filesystem::path &GetModexKitPath() { return kModexKitPath; }

std::optional<GearEntry> BuildGearEntry(RE::TESObjectARMO *a_armor) {
  if (!a_armor || a_armor->IsDeleted() || a_armor->IsIgnored() ||
      !a_armor->GetFile(0)) {
    return std::nullopt;
  }

  const auto editorID = sosr::armor::GetEditorID(a_armor);
  auto displayName = GetName(a_armor);
  if (displayName.empty()) {
    displayName = editorID;
  }
  if (displayName.empty()) {
    return std::nullopt;
  }

  GearEntry entry{};
  entry.formID = a_armor->GetFormID();
  entry.id = BuildEntryID(a_armor);
  entry.name = std::move(displayName);
  entry.editorID = editorID;
  entry.plugin = sosr::armor::GetPluginName(a_armor);
  entry.category = GetArmorCategory(a_armor);
  entry.slots = GetArmorSlots(a_armor);
  entry.slot = entry.slots.empty() ? std::string{} : entry.slots.front();
  entry.statValue = static_cast<int>(a_armor->GetArmorRating());
  entry.weight = a_armor->GetWeight();
  entry.value = a_armor->GetGoldValue();
  entry.keywords.insert(entry.keywords.end(), entry.slots.begin(),
                        entry.slots.end());
  entry.keywords.push_back(entry.category);
  SortUniqueStrings(entry.keywords);
  entry.keywordsText = JoinStrings(entry.keywords);
  entry.searchText = BuildGearSearchText(entry);
  return entry;
}

std::optional<OutfitEntry> BuildOutfitEntry(
    const RE::BGSOutfit *a_outfit,
    std::unordered_map<RE::FormID, ResolvedReferenceCollection>
        &a_leveledListCache) {
  if (!a_outfit || a_outfit->IsDeleted() || a_outfit->IsIgnored() ||
      !a_outfit->GetFile(0)) {
    return std::nullopt;
  }

  auto description = DescribeOutfit(a_outfit, a_leveledListCache);
  if (description.pieces.empty()) {
    return std::nullopt;
  }

  auto editorID = sosr::armor::GetEditorID(a_outfit);
  auto displayName = GetName(a_outfit);
  if (displayName.empty()) {
    displayName = editorID;
  }
  if (displayName.empty()) {
    displayName = "Form " + sosr::armor::FormatFormID(a_outfit->GetFormID());
  }

  OutfitEntry entry{};
  entry.formID = a_outfit->GetFormID();
  entry.id = BuildEntryID(a_outfit);
  entry.name = std::move(displayName);
  entry.editorID = std::move(editorID);
  entry.plugin = sosr::armor::GetPluginName(a_outfit);
  entry.summary = BuildOutfitSummary(description);
  entry.armorFormIDs = std::move(description.armorFormIDs);
  entry.itemTree = std::move(description.itemTree);
  entry.pieces = std::move(description.pieces);
  entry.slots = std::move(description.slots);
  entry.tags = std::move(description.tags);
  entry.piecesText = JoinStrings(entry.pieces);
  entry.slotsText = JoinStrings(entry.slots);
  entry.tagsText = JoinStrings(entry.tags);
  entry.searchText = BuildOutfitSearchText(entry);
  return entry;
}

std::optional<KitEntry> BuildKitEntry(const std::filesystem::path &a_path) {
  if (!std::filesystem::is_regular_file(a_path) ||
      a_path.extension() != ".json") {
    return std::nullopt;
  }

  const auto relativePath = a_path.lexically_relative(kModexKitPath);
  if (relativePath.string().starts_with("..")) {
    return std::nullopt;
  }

  const auto json = OpenJsonFile(a_path);
  if (!json.is_object() || json.size() != 1) {
    return std::nullopt;
  }

  const auto kitItem = json.items().begin();
  const auto &kitData = kitItem.value();
  if (!kitData.is_object()) {
    return std::nullopt;
  }

  const auto description =
      DescribeKitItems(kitData.value("Items", nlohmann::json::object()));
  if (description.hasMissingItems || description.armorFormIDs.empty()) {
    return std::nullopt;
  }

  auto name = kitItem.key();
  if (name.empty()) {
    name = a_path.stem().string();
  }

  KitEntry entry{};
  entry.id = "kit:" + relativePath.generic_string();
  entry.key = relativePath.generic_string();
  entry.name = std::move(name);
  entry.collection = relativePath.parent_path().generic_string();
  entry.filepath = a_path.string();
  entry.summary = BuildKitSummary(description);
  entry.armorFormIDs = description.armorFormIDs;
  entry.itemTree = std::move(description.itemTree);
  entry.pieces = description.pieces;
  entry.slots = description.slots;
  entry.piecesText = JoinStrings(entry.pieces);
  entry.slotsText = JoinStrings(entry.slots);
  entry.searchText = BuildKitSearchText(entry);
  return entry;
}
} // namespace sosr::catalog
