#include "VariantWorkbench.h"

#include "ArmorUtils.h"

#include <algorithm>
#include <unordered_set>

namespace sosng::workbench {
void VariantWorkbench::RebuildRowOrder() {
  rowOrder_.clear();
  rowOrder_.reserve(rows_.size());
  for (const auto &row : rows_) {
    rowOrder_.push_back(row.key);
  }
}

int VariantWorkbench::FindBestCatalogTargetRowIndex(
    const EquipmentWidgetItem &a_item, bool a_requireAcceptable) const {
  int fallbackRowIndex = -1;
  for (int rowIndex = 0; rowIndex < static_cast<int>(rows_.size());
       ++rowIndex) {
    const auto &row = rows_[static_cast<std::size_t>(rowIndex)];
    if (!row.isEquipped) {
      continue;
    }

    if (a_requireAcceptable && !CanAcceptOverride(rowIndex, a_item)) {
      continue;
    }

    if ((row.equipped.slotMask & a_item.slotMask) != 0) {
      return rowIndex;
    }

    if (fallbackRowIndex < 0) {
      fallbackRowIndex = rowIndex;
    }
  }

  return fallbackRowIndex;
}

void VariantWorkbench::SyncRowsFromPlayer() {
  auto *player = RE::PlayerCharacter::GetSingleton();
  if (!player) {
    for (auto &row : rows_) {
      row.isEquipped = false;
    }
    return;
  }

  for (auto &row : rows_) {
    row.isEquipped = false;
  }

  std::unordered_set<RE::FormID> seenArmorForms;

  for (const auto slot :
       {RE::BGSBipedObjectForm::BipedObjectSlot::kHead,
        RE::BGSBipedObjectForm::BipedObjectSlot::kHair,
        RE::BGSBipedObjectForm::BipedObjectSlot::kBody,
        RE::BGSBipedObjectForm::BipedObjectSlot::kHands,
        RE::BGSBipedObjectForm::BipedObjectSlot::kForearms,
        RE::BGSBipedObjectForm::BipedObjectSlot::kAmulet,
        RE::BGSBipedObjectForm::BipedObjectSlot::kRing,
        RE::BGSBipedObjectForm::BipedObjectSlot::kFeet,
        RE::BGSBipedObjectForm::BipedObjectSlot::kCalves,
        RE::BGSBipedObjectForm::BipedObjectSlot::kShield,
        RE::BGSBipedObjectForm::BipedObjectSlot::kTail,
        RE::BGSBipedObjectForm::BipedObjectSlot::kLongHair,
        RE::BGSBipedObjectForm::BipedObjectSlot::kCirclet,
        RE::BGSBipedObjectForm::BipedObjectSlot::kEars,
        RE::BGSBipedObjectForm::BipedObjectSlot::kModMouth,
        RE::BGSBipedObjectForm::BipedObjectSlot::kModNeck,
        RE::BGSBipedObjectForm::BipedObjectSlot::kModChestPrimary,
        RE::BGSBipedObjectForm::BipedObjectSlot::kModBack,
        RE::BGSBipedObjectForm::BipedObjectSlot::kModMisc1,
        RE::BGSBipedObjectForm::BipedObjectSlot::kModPelvisPrimary,
        RE::BGSBipedObjectForm::BipedObjectSlot::kDecapitateHead,
        RE::BGSBipedObjectForm::BipedObjectSlot::kDecapitate,
        RE::BGSBipedObjectForm::BipedObjectSlot::kModPelvisSecondary,
        RE::BGSBipedObjectForm::BipedObjectSlot::kModLegRight,
        RE::BGSBipedObjectForm::BipedObjectSlot::kModLegLeft,
        RE::BGSBipedObjectForm::BipedObjectSlot::kModFaceJewelry,
        RE::BGSBipedObjectForm::BipedObjectSlot::kModChestSecondary,
        RE::BGSBipedObjectForm::BipedObjectSlot::kModShoulder,
        RE::BGSBipedObjectForm::BipedObjectSlot::kModArmLeft,
        RE::BGSBipedObjectForm::BipedObjectSlot::kModArmRight,
        RE::BGSBipedObjectForm::BipedObjectSlot::kModMisc2,
        RE::BGSBipedObjectForm::BipedObjectSlot::kFX01}) {
    auto *armor = player->GetWornArmor(slot);
    if (!armor) {
      continue;
    }

    const auto formID = armor->GetFormID();
    if (!seenArmorForms.insert(formID).second) {
      continue;
    }

    EquipmentWidgetItem equipped{};
    if (!BuildCatalogItem(formID, equipped)) {
      continue;
    }

    const auto rowKey = "armor:" + armor::FormatFormID(formID);
    const auto existingIt =
        std::ranges::find(rows_, rowKey, [](const VariantWorkbenchRow &a_row) {
          return a_row.key;
        });
    if (existingIt != rows_.end()) {
      existingIt->equipped = std::move(equipped);
      existingIt->equipped.key = rowKey;
      existingIt->isEquipped = true;
      continue;
    }

    VariantWorkbenchRow row{};
    row.key = rowKey;
    row.equipped = std::move(equipped);
    row.equipped.key = rowKey;
    row.isEquipped = true;
    rows_.push_back(std::move(row));
    rowOrder_.push_back(rowKey);
  }
}

bool VariantWorkbench::BuildCatalogItem(RE::FormID a_formID,
                                        EquipmentWidgetItem &a_item) const {
  const auto *form = RE::TESForm::LookupByID(a_formID);
  if (!form) {
    return false;
  }

  a_item = {};
  a_item.formID = form->GetFormID();
  a_item.key = "form:" + armor::FormatFormID(form->GetFormID());
  a_item.name = armor::GetDisplayName(form);

  if (const auto *armorForm = form->As<RE::TESObjectARMO>()) {
    a_item.slotMask = armorForm->GetSlotMask().underlying();
    a_item.slotText =
        armor::JoinStrings(armor::GetArmorSlotLabels(a_item.slotMask));
    return true;
  }

  return false;
}

bool VariantWorkbench::CanAcceptOverride(int a_targetRowIndex,
                                         const EquipmentWidgetItem &a_item,
                                         int a_sourceRowIndex,
                                         int a_sourceItemIndex) const {
  if (a_targetRowIndex < 0 ||
      a_targetRowIndex >= static_cast<int>(rows_.size())) {
    return false;
  }

  if (a_sourceRowIndex == a_targetRowIndex && a_sourceItemIndex >= 0) {
    return false;
  }

  const auto &row = rows_[static_cast<std::size_t>(a_targetRowIndex)];
  for (int itemIndex = 0; itemIndex < static_cast<int>(row.overrides.size());
       ++itemIndex) {
    if (a_targetRowIndex == a_sourceRowIndex &&
        itemIndex == a_sourceItemIndex) {
      continue;
    }

    if (row.overrides[static_cast<std::size_t>(itemIndex)].formID ==
        a_item.formID) {
      return false;
    }
  }

  if (a_item.slotMask == 0) {
    return false;
  }
  return true;
}

bool VariantWorkbench::AddCatalogOverride(int a_targetRowIndex,
                                          RE::FormID a_formID) {
  EquipmentWidgetItem item{};
  if (!BuildCatalogItem(a_formID, item) ||
      !CanAcceptOverride(a_targetRowIndex, item)) {
    return false;
  }

  rows_[static_cast<std::size_t>(a_targetRowIndex)].overrides.push_back(
      std::move(item));
  return true;
}

bool VariantWorkbench::AddCatalogOverrideToBestRow(RE::FormID a_formID) {
  EquipmentWidgetItem item{};
  if (!BuildCatalogItem(a_formID, item)) {
    return false;
  }

  const auto targetRowIndex = FindBestCatalogTargetRowIndex(item, true);
  return targetRowIndex >= 0 ? AddCatalogOverride(targetRowIndex, a_formID)
                             : false;
}

bool VariantWorkbench::MoveOverride(int a_sourceRowIndex, int a_sourceItemIndex,
                                    int a_targetRowIndex) {
  if (a_sourceRowIndex < 0 ||
      a_sourceRowIndex >= static_cast<int>(rows_.size())) {
    return false;
  }

  auto &sourceOverrides =
      rows_[static_cast<std::size_t>(a_sourceRowIndex)].overrides;
  if (a_sourceItemIndex < 0 ||
      a_sourceItemIndex >= static_cast<int>(sourceOverrides.size())) {
    return false;
  }

  auto item = sourceOverrides[static_cast<std::size_t>(a_sourceItemIndex)];
  if (!CanAcceptOverride(a_targetRowIndex, item, a_sourceRowIndex,
                         a_sourceItemIndex)) {
    return false;
  }

  sourceOverrides.erase(sourceOverrides.begin() + a_sourceItemIndex);
  rows_[static_cast<std::size_t>(a_targetRowIndex)].overrides.push_back(
      std::move(item));
  return true;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
bool VariantWorkbench::DeleteOverride(int a_rowIndex, int a_itemIndex) {
  if (a_rowIndex < 0 || a_rowIndex >= static_cast<int>(rows_.size())) {
    return false;
  }

  auto &overrides = rows_[static_cast<std::size_t>(a_rowIndex)].overrides;
  if (a_itemIndex < 0 || a_itemIndex >= static_cast<int>(overrides.size())) {
    return false;
  }

  overrides.erase(overrides.begin() + a_itemIndex);
  return true;
}

bool VariantWorkbench::DeleteRow(int a_rowIndex) {
  if (a_rowIndex < 0 || a_rowIndex >= static_cast<int>(rows_.size())) {
    return false;
  }

  rows_.erase(rows_.begin() + a_rowIndex);
  RebuildRowOrder();
  return true;
}

bool VariantWorkbench::SetHideEquipped(int a_rowIndex, bool a_hideEquipped) {
  if (a_rowIndex < 0 || a_rowIndex >= static_cast<int>(rows_.size())) {
    return false;
  }

  rows_[static_cast<std::size_t>(a_rowIndex)].hideEquipped = a_hideEquipped;
  return true;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
bool VariantWorkbench::InsertCatalogRow(RE::FormID a_formID,
                                        int a_targetRowIndex,
                                        bool a_insertAfter) {
  if (a_targetRowIndex < 0 ||
      a_targetRowIndex >= static_cast<int>(rows_.size())) {
    return false;
  }

  EquipmentWidgetItem equipped{};
  if (!BuildCatalogItem(a_formID, equipped)) {
    return false;
  }

  const auto rowKey = "armor:" + armor::FormatFormID(a_formID);
  if (std::ranges::find(rows_, rowKey, [](const VariantWorkbenchRow &a_row) {
        return a_row.key;
      }) != rows_.end()) {
    return false;
  }

  VariantWorkbenchRow row{};
  row.key = rowKey;
  row.equipped = std::move(equipped);
  row.equipped.key = rowKey;

  auto insertIndex = a_targetRowIndex + (a_insertAfter ? 1 : 0);
  insertIndex = std::clamp(insertIndex, 0, static_cast<int>(rows_.size()));
  rows_.insert(rows_.begin() + insertIndex, std::move(row));

  RebuildRowOrder();
  return true;
}

bool VariantWorkbench::ApplyRowReorder(int a_sourceRowIndex,
                                       int a_targetRowIndex,
                                       bool a_insertAfter) {
  if (a_sourceRowIndex < 0 ||
      a_sourceRowIndex >= static_cast<int>(rows_.size())) {
    return false;
  }
  if (a_targetRowIndex < 0 ||
      a_targetRowIndex >= static_cast<int>(rows_.size())) {
    return false;
  }
  if (a_sourceRowIndex == a_targetRowIndex) {
    return false;
  }

  auto movedRow = std::move(rows_[static_cast<std::size_t>(a_sourceRowIndex)]);
  rows_.erase(rows_.begin() + a_sourceRowIndex);

  auto insertIndex = a_targetRowIndex + (a_insertAfter ? 1 : 0);
  if (a_sourceRowIndex < insertIndex) {
    --insertIndex;
  }

  rows_.insert(rows_.begin() + insertIndex, std::move(movedRow));

  RebuildRowOrder();
  return true;
}
} // namespace sosng::workbench
