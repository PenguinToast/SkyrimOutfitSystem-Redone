#pragma once

#include "EquipmentCatalog.h"

#include <filesystem>
#include <optional>
#include <unordered_map>

namespace sosr::catalog {
[[nodiscard]] const std::filesystem::path &GetModexKitPath();

[[nodiscard]] std::optional<GearEntry> BuildGearEntry(RE::TESObjectARMO *a_armor);
[[nodiscard]] std::optional<OutfitEntry> BuildOutfitEntry(
    const RE::BGSOutfit *a_outfit,
    std::unordered_map<RE::FormID, ResolvedReferenceCollection>
        &a_leveledListCache);
[[nodiscard]] std::optional<KitEntry>
BuildKitEntry(const std::filesystem::path &a_path);
} // namespace sosr::catalog
