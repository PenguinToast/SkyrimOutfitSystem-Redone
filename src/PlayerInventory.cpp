#include "PlayerInventory.h"

namespace sosr::player_inventory {
std::unordered_set<RE::FormID> GetInventoryArmorFormIDs() {
  std::unordered_set<RE::FormID> formIDs;

  auto *player = RE::PlayerCharacter::GetSingleton();
  if (!player) {
    return formIDs;
  }

  const auto inventory = player->GetInventory(
      [](RE::TESBoundObject &a_object) { return a_object.IsArmor(); });
  formIDs.reserve(inventory.size());

  for (const auto &[item, data] : inventory) {
    const auto &[count, entry] = data;
    (void)entry;
    if (item && count > 0) {
      formIDs.insert(item->GetFormID());
    }
  }

  return formIDs;
}
} // namespace sosr::player_inventory
