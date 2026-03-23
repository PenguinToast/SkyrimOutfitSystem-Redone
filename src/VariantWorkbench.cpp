#include "VariantWorkbench.h"

#include "ArmorUtils.h"
#include "EquipmentCatalog.h"

#include <algorithm>
#include <array>
#include <unordered_set>

namespace sosr::workbench {
namespace {
using BipedSlot = RE::BGSBipedObjectForm::BipedObjectSlot;

constexpr std::uint64_t SlotBit(const BipedSlot a_slot) {
  return static_cast<std::uint64_t>(std::to_underlying(a_slot));
}

std::string BuildSlotKey(const std::uint64_t a_slotMask) {
  const auto slotNumber = sosr::armor::GetArmorSlotNumber(a_slotMask);
  if (slotNumber == 0) {
    return {};
  }

  return "slot:" + std::to_string(slotNumber);
}

int ScorePreferredTargetSlots(
    const std::uint64_t a_itemMask, const std::uint64_t a_targetMask,
    const std::initializer_list<BipedSlot> &a_sourceSlots,
    const std::initializer_list<BipedSlot> &a_preferredTargetSlots) {
  for (const auto sourceSlot : a_sourceSlots) {
    if ((a_itemMask & SlotBit(sourceSlot)) == 0) {
      continue;
    }

    int score = static_cast<int>(a_preferredTargetSlots.size());
    for (const auto targetSlot : a_preferredTargetSlots) {
      if ((a_targetMask & SlotBit(targetSlot)) != 0) {
        return score;
      }
      --score;
    }
  }

  return -1;
}

int ScoreFallbackTargetRow(const std::uint64_t a_itemMask,
                           const std::uint64_t a_targetMask) {
  int bestScore = -1;

  const std::array scores{
      ScorePreferredTargetSlots(a_itemMask, a_targetMask, {BipedSlot::kCirclet},
                                {BipedSlot::kHead, BipedSlot::kHair,
                                 BipedSlot::kLongHair, BipedSlot::kEars}),
      ScorePreferredTargetSlots(a_itemMask, a_targetMask, {BipedSlot::kHead},
                                {BipedSlot::kCirclet, BipedSlot::kHair,
                                 BipedSlot::kLongHair, BipedSlot::kEars}),
      ScorePreferredTargetSlots(a_itemMask, a_targetMask,
                                {BipedSlot::kHair, BipedSlot::kLongHair,
                                 BipedSlot::kEars},
                                {BipedSlot::kHead, BipedSlot::kCirclet}),
      ScorePreferredTargetSlots(
          a_itemMask, a_targetMask, {BipedSlot::kAmulet, BipedSlot::kModNeck},
          {BipedSlot::kModNeck, BipedSlot::kAmulet, BipedSlot::kBody}),
      ScorePreferredTargetSlots(
          a_itemMask, a_targetMask, {BipedSlot::kRing},
          {BipedSlot::kModFaceJewelry, BipedSlot::kCirclet, BipedSlot::kEars}),
      ScorePreferredTargetSlots(
          a_itemMask, a_targetMask, {BipedSlot::kHands},
          {BipedSlot::kForearms, BipedSlot::kBody}),
      ScorePreferredTargetSlots(
          a_itemMask, a_targetMask, {BipedSlot::kForearms},
          {BipedSlot::kHands, BipedSlot::kBody}),
      ScorePreferredTargetSlots(
          a_itemMask, a_targetMask, {BipedSlot::kFeet},
          {BipedSlot::kCalves, BipedSlot::kBody}),
      ScorePreferredTargetSlots(
          a_itemMask, a_targetMask, {BipedSlot::kCalves},
          {BipedSlot::kFeet, BipedSlot::kBody}),
      ScorePreferredTargetSlots(
          a_itemMask, a_targetMask,
          {BipedSlot::kModFaceJewelry, BipedSlot::kModMouth},
          {BipedSlot::kHead, BipedSlot::kCirclet, BipedSlot::kEars}),
      ScorePreferredTargetSlots(
          a_itemMask, a_targetMask,
          {BipedSlot::kModChestPrimary, BipedSlot::kModChestSecondary,
           BipedSlot::kModBack, BipedSlot::kModShoulder,
           BipedSlot::kModPelvisPrimary, BipedSlot::kModPelvisSecondary},
          {BipedSlot::kBody}),
      ScorePreferredTargetSlots(
          a_itemMask, a_targetMask,
          {BipedSlot::kModArmLeft, BipedSlot::kModArmRight},
          {BipedSlot::kHands, BipedSlot::kForearms, BipedSlot::kBody}),
      ScorePreferredTargetSlots(
          a_itemMask, a_targetMask,
          {BipedSlot::kModLegLeft, BipedSlot::kModLegRight},
          {BipedSlot::kFeet, BipedSlot::kCalves, BipedSlot::kBody}),
  };

  for (const auto score : scores) {
    bestScore = (std::max)(bestScore, score);
  }

  return bestScore;
}
} // namespace

bool VariantWorkbench::ResolveCatalogArmors(
    const std::vector<RE::FormID> &a_formIDs,
    std::vector<const RE::TESObjectARMO *> &a_armors) const {
  a_armors.clear();

  for (const auto formID : EquipmentCatalog::Get().ResolveArmorFormIDs(a_formIDs)) {
    const auto *armor = RE::TESForm::LookupByID<RE::TESObjectARMO>(formID);
    if (!armor) {
      continue;
    }
    a_armors.push_back(armor);
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
  int bestPrecedenceRowIndex = -1;
  int bestPrecedenceScore = -1;
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

    const auto precedenceScore =
        ScoreFallbackTargetRow(a_item.slotMask, row.equipped.slotMask);
    if (precedenceScore > bestPrecedenceScore) {
      bestPrecedenceScore = precedenceScore;
      bestPrecedenceRowIndex = rowIndex;
    }
  }

  if (bestPrecedenceRowIndex >= 0) {
    return bestPrecedenceRowIndex;
  }

  return fallbackRowIndex;
}

int VariantWorkbench::FindBestCatalogTargetRowIndex(
    const EquipmentWidgetItem &a_item, bool a_requireAcceptable) const {
  return FindBestCatalogTargetRowIndex(a_item, a_requireAcceptable, nullptr);
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
  std::vector<VariantWorkbenchRow> newlyEquippedRows;
  std::vector<std::string> newlyEquippedRowKeys;

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
    newlyEquippedRowKeys.push_back(rowKey);
    newlyEquippedRows.push_back(std::move(row));
  }

  if (!newlyEquippedRows.empty()) {
    rows_.insert(rows_.begin(),
                 std::make_move_iterator(newlyEquippedRows.begin()),
                 std::make_move_iterator(newlyEquippedRows.end()));
    rowOrder_.insert(rowOrder_.begin(),
                     std::make_move_iterator(newlyEquippedRowKeys.begin()),
                     std::make_move_iterator(newlyEquippedRowKeys.end()));
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

bool VariantWorkbench::BuildSlotItem(const std::uint64_t a_slotMask,
                                     EquipmentWidgetItem &a_item) const {
  const auto slotNumber = armor::GetArmorSlotNumber(a_slotMask);
  if (slotNumber == 0) {
    return false;
  }

  a_item = {};
  a_item.formID = 0;
  a_item.key = BuildSlotKey(a_slotMask);
  a_item.name = armor::JoinStrings(armor::GetArmorSlotLabels(a_slotMask));
  a_item.slotText = "Equipment slot override";
  a_item.slotMask = a_slotMask;
  return true;
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

bool VariantWorkbench::AddCatalogSelectionAsRows(
    const std::vector<RE::FormID> &a_formIDs) {
  auto newRows = BuildCatalogRows(a_formIDs);
  bool addedAny = false;
  for (auto &row : newRows) {
    rowOrder_.push_back(row.key);
    rows_.push_back(std::move(row));
    addedAny = true;
  }

  return addedAny;
}

bool VariantWorkbench::AddSlotRow(const std::uint64_t a_slotMask) {
  auto row = BuildSlotRow(a_slotMask);
  if (!row) {
    return false;
  }

  rowOrder_.push_back(row->key);
  rows_.push_back(std::move(*row));
  return true;
}

std::vector<VariantWorkbenchRow> VariantWorkbench::BuildCatalogRows(
    const std::vector<RE::FormID> &a_formIDs) const {
  std::vector<VariantWorkbenchRow> newRows;

  std::vector<const RE::TESObjectARMO *> armors;
  if (!ResolveCatalogArmors(a_formIDs, armors)) {
    return newRows;
  }

  std::unordered_set<std::string> seenRowKeys;
  for (const auto &row : rows_) {
    seenRowKeys.insert(row.key);
  }

  for (const auto *armor : armors) {
    if (!armor) {
      continue;
    }

    const auto rowKey = "armor:" + armor::FormatFormID(armor->GetFormID());
    if (!seenRowKeys.insert(rowKey).second) {
      continue;
    }

    EquipmentWidgetItem equipped{};
    if (!BuildCatalogItem(armor->GetFormID(), equipped)) {
      continue;
    }

    VariantWorkbenchRow row{};
    row.key = rowKey;
    row.equipped = std::move(equipped);
    row.equipped.key = rowKey;
    newRows.push_back(std::move(row));
  }

  return newRows;
}

std::optional<VariantWorkbenchRow>
VariantWorkbench::BuildSlotRow(const std::uint64_t a_slotMask) const {
  const auto rowKey = BuildSlotKey(a_slotMask);
  if (rowKey.empty()) {
    return std::nullopt;
  }

  if (std::ranges::find(rows_, rowKey, &VariantWorkbenchRow::key) !=
      rows_.end()) {
    return std::nullopt;
  }

  EquipmentWidgetItem slotItem{};
  if (!BuildSlotItem(a_slotMask, slotItem)) {
    return std::nullopt;
  }

  VariantWorkbenchRow row{};
  row.key = rowKey;
  row.kind = VariantWorkbenchRowKind::EquipmentSlot;
  row.equipped = std::move(slotItem);
  row.equipped.key = rowKey;
  return row;
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

  auto newRows = BuildCatalogRows(std::vector<RE::FormID>{a_formID});
  if (newRows.empty()) {
    return false;
  }

  auto insertIndex = a_targetRowIndex + (a_insertAfter ? 1 : 0);
  insertIndex = std::clamp(insertIndex, 0, static_cast<int>(rows_.size()));
  rows_.insert(rows_.begin() + insertIndex, std::move(newRows.front()));

  RebuildRowOrder();
  return true;
}

bool VariantWorkbench::InsertSlotRow(const std::uint64_t a_slotMask,
                                     int a_targetRowIndex,
                                     bool a_insertAfter) {
  if (a_targetRowIndex < 0 ||
      a_targetRowIndex >= static_cast<int>(rows_.size())) {
    return false;
  }

  auto row = BuildSlotRow(a_slotMask);
  if (!row) {
    return false;
  }

  auto insertIndex = a_targetRowIndex + (a_insertAfter ? 1 : 0);
  insertIndex = std::clamp(insertIndex, 0, static_cast<int>(rows_.size()));
  rows_.insert(rows_.begin() + insertIndex, std::move(*row));

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
