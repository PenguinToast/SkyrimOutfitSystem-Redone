#pragma once

#include <unordered_set>

namespace sosr::player_inventory {
[[nodiscard]] std::unordered_set<RE::FormID> GetInventoryArmorFormIDs();
} // namespace sosr::player_inventory
