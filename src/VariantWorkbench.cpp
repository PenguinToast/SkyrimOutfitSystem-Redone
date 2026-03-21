#include "VariantWorkbench.h"

#include "ArmorUtils.h"

#include <algorithm>
#include <unordered_set>

namespace sosr::workbench {
bool VariantWorkbench::ResolveCatalogArmors(
    RE::FormID a_formID,
    std::vector<const RE::TESObjectARMO *> &a_armors) const {
  const auto *form = RE::TESForm::LookupByID(a_formID);
  if (!form) {
    return false;
  }

  if (const auto *armor = form->As<RE::TESObjectARMO>()) {
    return ResolveCatalogArmors(std::vector<RE::FormID>{armor->GetFormID()},
                                a_armors);
  }

  const auto *outfit = form->As<RE::BGSOutfit>();
  if (!outfit) {
    return false;
  }

  std::vector<RE::FormID> armorFormIDs;
  outfit->ForEachItem([&](RE::TESForm *a_item) {
    const auto *armor = a_item ? a_item->As<RE::TESObjectARMO>() : nullptr;
    if (!armor) {
      return RE::BSContainer::ForEachResult::kContinue;
    }

    armorFormIDs.push_back(armor->GetFormID());
    return RE::BSContainer::ForEachResult::kContinue;
  });

  return ResolveCatalogArmors(armorFormIDs, a_armors);
}

bool VariantWorkbench::ResolveCatalogArmors(
    const std::vector<RE::FormID> &a_formIDs,
    std::vector<const RE::TESObjectARMO *> &a_armors) const {
  a_armors.clear();

  std::unordered_set<RE::FormID> seenForms;
  for (const auto formID : a_formIDs) {
    const auto *armor = RE::TESForm::LookupByID<RE::TESObjectARMO>(formID);
    if (!armor) {
      continue;
    }

    if (seenForms.insert(formID).second) {
      a_armors.push_back(armor);
    }
  }

  return !a_armors.empty();
}

bool VariantWorkbench::CanAcceptOverrideWithPendingAssignments(
    int a_targetRowIndex, const EquipmentWidgetItem &a_item,
    const std::vector<PlannedCatalogAssignment> &a_pendingAssignments) const {
  if (!CanAcceptOverride(a_targetRowIndex, a_item)) {
    return false;
  }

  for (const auto &assignment : a_pendingAssignments) {
    if (assignment.rowIndex == a_targetRowIndex &&
        assignment.armorFormID == a_item.formID) {
      return false;
    }
  }

  return true;
}

void VariantWorkbench::RebuildRowOrder() {
  rowOrder_.clear();
  rowOrder_.reserve(rows_.size());
  for (const auto &row : rows_) {
    rowOrder_.push_back(row.key);
  }
}

int VariantWorkbench::FindBestCatalogTargetRowIndex(
    const EquipmentWidgetItem &a_item, bool a_requireAcceptable,
    const std::vector<PlannedCatalogAssignment> *a_pendingAssignments) const {
  int fallbackRowIndex = -1;
  for (int rowIndex = 0; rowIndex < static_cast<int>(rows_.size());
       ++rowIndex) {
    const auto &row = rows_[static_cast<std::size_t>(rowIndex)];
    if (!row.isEquipped) {
      continue;
    }

    if (a_requireAcceptable &&
        ((a_pendingAssignments &&
          !CanAcceptOverrideWithPendingAssignments(rowIndex, a_item,
                                                   *a_pendingAssignments)) ||
         (!a_pendingAssignments && !CanAcceptOverride(rowIndex, a_item)))) {
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

int VariantWorkbench::FindBestCatalogTargetRowIndex(
    const EquipmentWidgetItem &a_item, bool a_requireAcceptable) const {
  return FindBestCatalogTargetRowIndex(a_item, a_requireAcceptable, nullptr);
}

bool VariantWorkbench::PlanCatalogAssignments(
    RE::FormID a_formID,
    std::vector<PlannedCatalogAssignment> &a_assignments) const {
  std::vector<const RE::TESObjectARMO *> armors;
  if (!ResolveCatalogArmors(a_formID, armors)) {
    return false;
  }

  return PlanCatalogAssignments(
      [&]() {
        std::vector<RE::FormID> armorFormIDs;
        armorFormIDs.reserve(armors.size());
        for (const auto *armor : armors) {
          armorFormIDs.push_back(armor->GetFormID());
        }
        return armorFormIDs;
      }(),
      a_assignments);
}

bool VariantWorkbench::PlanCatalogAssignments(
    const std::vector<RE::FormID> &a_formIDs,
    std::vector<PlannedCatalogAssignment> &a_assignments) const {
  a_assignments.clear();

  std::vector<const RE::TESObjectARMO *> armors;
  if (!ResolveCatalogArmors(a_formIDs, armors)) {
    return false;
  }

  for (const auto *armor : armors) {
    EquipmentWidgetItem item{};
    if (!armor || !BuildCatalogItem(armor->GetFormID(), item)) {
      continue;
    }

    const auto rowIndex =
        FindBestCatalogTargetRowIndex(item, true, &a_assignments);
    if (rowIndex < 0) {
      continue;
    }

    a_assignments.push_back({rowIndex, armor->GetFormID()});
  }

  return !a_assignments.empty();
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

bool VariantWorkbench::IsPreviewingSelection(
    std::string_view a_selectionKey) const {
  return previewSelectionKey_ == a_selectionKey && !previewDavVariants_.empty();
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

bool VariantWorkbench::AddCatalogSelectionToWorkbench(RE::FormID a_formID) {
  std::vector<PlannedCatalogAssignment> assignments;
  if (!PlanCatalogAssignments(a_formID, assignments)) {
    return false;
  }

  bool addedAny = false;
  for (const auto &assignment : assignments) {
    addedAny |= AddCatalogOverride(assignment.rowIndex, assignment.armorFormID);
  }

  return addedAny;
}

bool VariantWorkbench::AddCatalogSelectionToWorkbench(
    const std::vector<RE::FormID> &a_formIDs) {
  std::vector<PlannedCatalogAssignment> assignments;
  if (!PlanCatalogAssignments(a_formIDs, assignments)) {
    return false;
  }

  bool addedAny = false;
  for (const auto &assignment : assignments) {
    addedAny |= AddCatalogOverride(assignment.rowIndex, assignment.armorFormID);
  }

  return addedAny;
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

bool VariantWorkbench::ResetEquippedRows() {
  bool changed = false;
  for (auto &row : rows_) {
    if (!row.isEquipped) {
      continue;
    }

    if (!row.overrides.empty()) {
      row.overrides.clear();
      changed = true;
    }
    if (row.hideEquipped) {
      row.hideEquipped = false;
      changed = true;
    }
  }

  return changed;
}

void VariantWorkbench::ResetAllRows() { Revert(); }

std::vector<RE::FormID> VariantWorkbench::CollectEquippedArmorFormIDs() const {
  std::vector<RE::FormID> formIDs;
  std::unordered_set<RE::FormID> seen;
  formIDs.reserve(rows_.size());

  for (const auto &row : rows_) {
    if (!row.isEquipped || row.equipped.formID == 0) {
      continue;
    }
    if (seen.insert(row.equipped.formID).second) {
      formIDs.push_back(row.equipped.formID);
    }
  }

  return formIDs;
}

std::vector<RE::FormID>
VariantWorkbench::CollectOverrideArmorFormIDsFromEquippedRows() const {
  std::vector<RE::FormID> formIDs;
  std::unordered_set<RE::FormID> seen;

  for (const auto &row : rows_) {
    if (!row.isEquipped) {
      continue;
    }

    for (const auto &item : row.overrides) {
      if (item.formID == 0) {
        continue;
      }
      if (seen.insert(item.formID).second) {
        formIDs.push_back(item.formID);
      }
    }
  }

  return formIDs;
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
} // namespace sosr::workbench
