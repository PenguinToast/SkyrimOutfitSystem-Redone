#include "VariantWorkbench.h"

#include "ArmorUtils.h"
#include "ConditionMaterializer.h"
#include "ConditionRefreshTargets.h"
#include "integrations/DynamicArmorVariantsExtendedClient.h"

#include <nlohmann/json.hpp>
#include <unordered_set>

namespace {
constexpr std::uint32_t kSerializationType = 'ROWS';
constexpr std::uint32_t kSerializationVersion = 4;

auto GetDynamicArmorVariantsExtendedClient()
    -> DynamicArmorVariantsExtendedAPI::
        IDynamicArmorVariantsExtendedInterface001 * {
  auto *dav = sosr::integrations::DynamicArmorVariantsExtendedClient::Get();
  if (dav) {
    return dav;
  }

  sosr::integrations::DynamicArmorVariantsExtendedClient::Refresh();
  return sosr::integrations::DynamicArmorVariantsExtendedClient::Get();
}

std::string BuildPreviewVariantName(const std::string &a_rowKey) {
  return "SOSR|PreviewSelected|" + a_rowKey;
}

std::string BuildRowKey(const std::string_view a_sourceKey,
                        const std::optional<std::string> &a_conditionId) {
  std::string key(a_sourceKey);
  key.append("|condition:");
  if (a_conditionId.has_value()) {
    key.append(*a_conditionId);
  } else {
    key.append("null");
  }
  return key;
}

auto BuildDavVariantPriority(const std::size_t a_rowIndex,
                             const std::size_t a_rowCount) -> std::int32_t {
  if (a_rowCount == 0 || a_rowIndex >= a_rowCount) {
    return 0;
  }

  return static_cast<std::int32_t>(a_rowCount - a_rowIndex);
}

struct DavReplacementData {
  std::vector<std::string> replacements;
  std::string displayName;
};

struct DavVariantDescriptor {
  std::string name;
  std::string json;
};

struct DavVariantPayload {
  std::string variantJson;
  std::string conditionSignature;
  std::shared_ptr<RE::TESCondition> condition;
  sosr::conditions::RefreshTargets refreshTargets;
};

void UpdateRowIdentity(sosr::workbench::VariantWorkbenchRow &a_row) {
  a_row.key = BuildRowKey(a_row.sourceKey, a_row.conditionId);
  a_row.equipped.key = a_row.key;
}

auto CollectDavReplacementData(
    const std::vector<const RE::TESObjectARMO *> &a_overrideArmors)
    -> DavReplacementData {
  DavReplacementData data;
  std::unordered_set<std::string> replacementSet;

  for (const auto *overrideArmor : a_overrideArmors) {
    if (!overrideArmor) {
      continue;
    }

    if (!data.displayName.empty()) {
      data.displayName.append(" + ");
    }
    data.displayName.append(sosr::armor::GetDisplayName(overrideArmor));

    for (const auto *overrideAddon : overrideArmor->armorAddons) {
      if (!overrideAddon) {
        continue;
      }

      const auto identifier =
          sosr::armor::GetReplacementIdentifier(overrideArmor, overrideAddon);
      if (identifier.empty()) {
        continue;
      }

      if (replacementSet.insert(identifier).second) {
        data.replacements.push_back(identifier);
      }
    }
  }

  return data;
}

auto BuildDavVariantJson(
    const RE::TESObjectARMO *a_sourceArmor,
    const std::vector<const RE::TESObjectARMO *> &a_overrideArmors,
    bool a_hideEquipped, const std::int32_t a_priority) -> std::string {
  nlohmann::json replaceByForm = nlohmann::json::object();
  const auto replacementData = CollectDavReplacementData(a_overrideArmors);
  if (replacementData.replacements.empty() && !a_hideEquipped) {
    return {};
  }

  for (const auto *sourceAddon : a_sourceArmor->armorAddons) {
    if (!sourceAddon) {
      continue;
    }

    const auto sourceIdentifier = sosr::armor::GetFormIdentifier(sourceAddon);
    if (sourceIdentifier.empty()) {
      continue;
    }

    if (a_hideEquipped) {
      replaceByForm[sourceIdentifier] = nlohmann::json::array();
    } else if (replacementData.replacements.size() == 1) {
      replaceByForm[sourceIdentifier] = replacementData.replacements.front();
    } else {
      replaceByForm[sourceIdentifier] = replacementData.replacements;
    }
  }

  if (replaceByForm.empty()) {
    return {};
  }

  nlohmann::json root{
      {"displayName",
       a_hideEquipped ? "Hidden " + sosr::armor::GetDisplayName(a_sourceArmor)
                      : (replacementData.displayName.empty()
                             ? sosr::armor::GetDisplayName(a_sourceArmor)
                             : replacementData.displayName)},
      {"priority", a_priority},
      {"replaceByForm", replaceByForm}};
  return root.dump();
}

auto BuildDavSlotVariantJson(
    const std::uint64_t a_slotMask,
    const std::vector<const RE::TESObjectARMO *> &a_overrideArmors,
    const bool a_hideEquipped, const std::int32_t a_priority) -> std::string {
  const auto slotNumber = sosr::armor::GetArmorSlotNumber(a_slotMask);
  if (slotNumber == 0) {
    return {};
  }

  const auto replacementData = CollectDavReplacementData(a_overrideArmors);
  if (replacementData.replacements.empty() && !a_hideEquipped) {
    return {};
  }

  nlohmann::json replaceBySlot = nlohmann::json::object();
  if (a_hideEquipped) {
    replaceBySlot[std::to_string(slotNumber)] = nlohmann::json::array();
  } else if (replacementData.replacements.size() == 1) {
    replaceBySlot[std::to_string(slotNumber)] =
        replacementData.replacements.front();
  } else {
    replaceBySlot[std::to_string(slotNumber)] = replacementData.replacements;
  }

  const auto slotLabel =
      sosr::armor::JoinStrings(sosr::armor::GetArmorSlotLabels(a_slotMask));
  nlohmann::json root{
      {"displayName", a_hideEquipped ? "Hidden " + slotLabel
                                     : (replacementData.displayName.empty()
                                            ? slotLabel
                                            : replacementData.displayName)},
      {"priority", a_priority},
      {"replaceBySlot", replaceBySlot}};
  return root.dump();
}

auto BuildDavVariantDescriptor(
    const sosr::workbench::VariantWorkbenchRow &a_row,
    const std::vector<const RE::TESObjectARMO *> &a_overrideArmors,
    const std::int32_t a_priority) -> std::optional<DavVariantDescriptor> {
  DavVariantDescriptor descriptor;
  descriptor.name = "SOSR|Row|" + a_row.key;

  if (a_row.IsSlotRow()) {
    descriptor.json =
        BuildDavSlotVariantJson(a_row.equipped.slotMask, a_overrideArmors,
                                a_row.hideEquipped, a_priority);
  } else {
    const auto *sourceArmor =
        RE::TESForm::LookupByID<RE::TESObjectARMO>(a_row.equipped.formID);
    if (!sourceArmor) {
      return std::nullopt;
    }

    descriptor.json = BuildDavVariantJson(sourceArmor, a_overrideArmors,
                                          a_row.hideEquipped, a_priority);
  }

  if (descriptor.name.empty() || descriptor.json.empty()) {
    return std::nullopt;
  }

  return descriptor;
}

bool SerializeRowSource(const sosr::workbench::VariantWorkbenchRow &a_row,
                        nlohmann::json &a_serializedRow) {
  a_serializedRow["type"] = a_row.IsSlotRow() ? "slot" : "armor";
  if (a_row.IsSlotRow()) {
    const auto slotNumber =
        sosr::armor::GetArmorSlotNumber(a_row.equipped.slotMask);
    if (slotNumber == 0) {
      return false;
    }
    a_serializedRow["slot"] = slotNumber;
    return true;
  }

  a_serializedRow["equipped"] = sosr::armor::GetFormIdentifier(
      RE::TESForm::LookupByID(a_row.equipped.formID));
  return !a_serializedRow["equipped"].get_ref<const std::string &>().empty();
}

bool DeserializeRowSource(const nlohmann::json &a_serializedRow,
                          sosr::workbench::VariantWorkbench &a_workbench,
                          sosr::workbench::VariantWorkbenchRow &a_row) {
  const auto rowType = a_serializedRow.value("type", std::string{"armor"});
  if (rowType == "slot") {
    const auto slotMask =
        sosr::armor::GetArmorSlotMask(a_serializedRow.value("slot", 0));
    sosr::workbench::EquipmentWidgetItem slotItem{};
    if (slotMask == 0 || !a_workbench.BuildSlotItem(slotMask, slotItem)) {
      return false;
    }

    a_row.sourceKey = slotItem.key;
    a_row.equipped = std::move(slotItem);
    return true;
  }

  const auto *equippedForm = sosr::armor::LookupByIdentifier<RE::TESObjectARMO>(
      a_serializedRow.value("equipped", std::string{}));
  sosr::workbench::EquipmentWidgetItem equipped{};
  if (!equippedForm ||
      !a_workbench.BuildCatalogItem(equippedForm->GetFormID(), equipped)) {
    return false;
  }

  a_row.sourceKey =
      "armor:" + sosr::armor::FormatFormID(equippedForm->GetFormID());
  a_row.equipped = std::move(equipped);
  return true;
}
} // namespace

namespace sosr::workbench {
bool VariantWorkbench::ApplyCatalogPreview(
    std::string_view a_selectionKey, const std::vector<RE::FormID> &a_formIDs) {
  std::vector<PlannedCatalogAssignment> assignments;
  if (!PlanCatalogAssignments(a_formIDs, assignments)) {
    ClearPreview();
    return false;
  }

  std::unordered_map<std::string, std::vector<const RE::TESObjectARMO *>>
      previewOverridesByRow;
  previewOverridesByRow.reserve(assignments.size());

  for (const auto &assignment : assignments) {
    const auto &targetRow =
        rows_[static_cast<std::size_t>(assignment.rowIndex)];
    const auto *overrideArmor =
        RE::TESForm::LookupByID<RE::TESObjectARMO>(assignment.armorFormID);
    if (!overrideArmor) {
      continue;
    }

    previewOverridesByRow[targetRow.key].push_back(overrideArmor);
  }

  if (previewOverridesByRow.empty()) {
    ClearPreview();
    return false;
  }

  std::unordered_map<std::string, std::string> desiredPreviewVariants;
  desiredPreviewVariants.reserve(previewOverridesByRow.size());

  for (const auto &[rowKey, overrideArmors] : previewOverridesByRow) {
    const auto rowIt =
        std::ranges::find(rows_, rowKey, [](const VariantWorkbenchRow &a_row) {
          return a_row.key;
        });
    if (rowIt == rows_.end()) {
      continue;
    }

    const auto rowIndex =
        static_cast<std::size_t>(std::distance(rows_.begin(), rowIt));

    const auto *sourceArmor =
        RE::TESForm::LookupByID<RE::TESObjectARMO>(rowIt->equipped.formID);
    if (!sourceArmor) {
      continue;
    }

    const auto variantJson =
        BuildDavVariantJson(sourceArmor, overrideArmors, false,
                            BuildDavVariantPriority(rowIndex, rows_.size()));
    if (variantJson.empty()) {
      continue;
    }

    desiredPreviewVariants.emplace(BuildPreviewVariantName(rowKey),
                                   variantJson);
  }

  if (desiredPreviewVariants.empty()) {
    ClearPreview();
    return false;
  }

  if (previewSelectionKey_ == a_selectionKey &&
      previewDavVariants_ == desiredPreviewVariants) {
    return true;
  }

  auto *dav = GetDynamicArmorVariantsExtendedClient();
  if (!(dav = sosr::integrations::DynamicArmorVariantsExtendedClient::
            GetReady())) {
    ClearPreview();
    return false;
  }

  auto *player = RE::PlayerCharacter::GetSingleton();
  if (!player) {
    ClearPreview();
    return false;
  }

  ClearPreview();

  std::unordered_map<std::string, std::string> appliedPreviewVariants;
  appliedPreviewVariants.reserve(desiredPreviewVariants.size());
  for (const auto &[variantName, variantJson] : desiredPreviewVariants) {
    if (!dav->RegisterVariantJson(variantName.c_str(), variantJson.c_str())) {
      logger::warn("Failed to register SOSR preview variant {}", variantName);
      for (const auto &[appliedVariantName, _] : appliedPreviewVariants) {
        dav->RemoveVariantOverride(player, appliedVariantName.c_str());
        dav->DeleteVariant(appliedVariantName.c_str());
      }
      ClearPreview();
      return false;
    }

    if (!dav->ApplyVariantOverride(player, variantName.c_str(), true)) {
      logger::warn("Failed to apply SOSR preview variant override {}",
                   variantName);
      dav->DeleteVariant(variantName.c_str());
      for (const auto &[appliedVariantName, _] : appliedPreviewVariants) {
        dav->RemoveVariantOverride(player, appliedVariantName.c_str());
        dav->DeleteVariant(appliedVariantName.c_str());
      }
      ClearPreview();
      return false;
    }

    appliedPreviewVariants.emplace(variantName, variantJson);
  }

  previewSelectionKey_ = a_selectionKey;
  previewDavVariants_ = std::move(desiredPreviewVariants);
  return true;
}

void VariantWorkbench::ClearPreview() {
  if (previewSelectionKey_.empty() && previewDavVariants_.empty()) {
    return;
  }

  auto *dav = GetDynamicArmorVariantsExtendedClient();
  auto *player = RE::PlayerCharacter::GetSingleton();
  bool variantsChanged = false;

  if (dav) {
    if (player) {
      for (const auto &[variantName, _] : previewDavVariants_) {
        if (!dav->RemoveVariantOverride(player, variantName.c_str())) {
          logger::warn("Failed to remove SOSR preview variant override {}",
                       variantName);
        }
      }
    }
    for (const auto &[variantName, _] : previewDavVariants_) {
      if (!dav->DeleteVariant(variantName.c_str())) {
        logger::warn("Failed to delete SOSR preview variant {}", variantName);
      } else {
        variantsChanged = true;
      }
    }
    if (variantsChanged && player) {
      dav->RefreshActor(player);
    }
  }

  previewSelectionKey_.clear();
  previewDavVariants_.clear();
}

void VariantWorkbench::SyncDynamicArmorVariantsExtended(
    std::vector<ui::conditions::Definition> &a_conditions) {
  auto *dav =
      sosr::integrations::DynamicArmorVariantsExtendedClient::GetReady();
  if (!dav) {
    return;
  }

  std::unordered_map<std::string, DavVariantPayload> desiredVariants;
  desiredVariants.reserve(rows_.size());

  for (std::size_t rowIndex = 0; rowIndex < rows_.size(); ++rowIndex) {
    const auto &row = rows_[rowIndex];
    std::vector<const RE::TESObjectARMO *> overrideArmors;
    overrideArmors.reserve(row.overrides.size());
    for (const auto &overrideItem : row.overrides) {
      if (const auto *overrideArmor =
              RE::TESForm::LookupByID<RE::TESObjectARMO>(overrideItem.formID)) {
        overrideArmors.push_back(overrideArmor);
      }
    }

    if (overrideArmors.empty() && !row.hideEquipped) {
      continue;
    }
    if (!row.conditionId.has_value()) {
      continue;
    }

    const auto descriptor = BuildDavVariantDescriptor(
        row, overrideArmors, BuildDavVariantPriority(rowIndex, rows_.size()));
    if (!descriptor.has_value()) {
      continue;
    }

    const auto materializedCondition =
        sosr::conditions::MaterializeConditionById(*row.conditionId,
                                                   a_conditions);
    if (!materializedCondition.has_value()) {
      logger::warn("Failed to materialize SVS condition {} for row {}",
                   *row.conditionId, row.key);
      continue;
    }

    desiredVariants.insert_or_assign(
        descriptor->name,
        DavVariantPayload{
            .variantJson = std::move(descriptor->json),
            .conditionSignature = materializedCondition->signature,
            .condition = materializedCondition->condition,
            .refreshTargets = materializedCondition->refreshTargets});
  }

  std::unordered_map<std::string, ActiveDavVariantState> syncedVariants;
  syncedVariants.reserve(desiredVariants.size());
  bool variantsChanged = false;
  sosr::conditions::RefreshTargets refreshTargets;

  for (const auto &[variantName, payload] : desiredVariants) {
    if (const auto activeIt = activeDavVariants_.find(variantName);
        activeIt != activeDavVariants_.end() &&
        activeIt->second.variantJson == payload.variantJson &&
        activeIt->second.conditionSignature == payload.conditionSignature) {
      syncedVariants.emplace(variantName,
                             ActiveDavVariantState{payload.variantJson,
                                                   payload.conditionSignature,
                                                   payload.refreshTargets});
      continue;
    }

    const auto activeIt = activeDavVariants_.find(variantName);
    if (!dav->RegisterVariantJson(variantName.c_str(),
                                  payload.variantJson.c_str())) {
      logger::warn("Failed to register DAV variant {}", variantName);
      continue;
    }

    if (!dav->SetCondition(variantName.c_str(), payload.condition)) {
      logger::warn("Failed to set DAV condition for {}", variantName);
      dav->DeleteVariant(variantName.c_str());
      continue;
    }

    syncedVariants.emplace(variantName,
                           ActiveDavVariantState{payload.variantJson,
                                                 payload.conditionSignature,
                                                 payload.refreshTargets});
    if (activeIt != activeDavVariants_.end()) {
      sosr::conditions::MergeRefreshTargets(refreshTargets,
                                            activeIt->second.refreshTargets);
    }
    sosr::conditions::MergeRefreshTargets(refreshTargets,
                                          payload.refreshTargets);
    variantsChanged = true;
  }

  for (const auto &[variantName, state] : activeDavVariants_) {
    if (syncedVariants.contains(variantName)) {
      continue;
    }

    if (!dav->DeleteVariant(variantName.c_str())) {
      logger::warn("Failed to delete DAV variant {}", variantName);
    } else {
      sosr::conditions::MergeRefreshTargets(refreshTargets,
                                            state.refreshTargets);
      variantsChanged = true;
    }
  }

  activeDavVariants_ = std::move(syncedVariants);

  if (variantsChanged) {
    sosr::conditions::RefreshActors(*dav, refreshTargets);
  }
}

void VariantWorkbench::Serialize(SKSE::SerializationInterface *a_skse) const {
  nlohmann::json root;
  root["rows"] = nlohmann::json::array();

  for (const auto &row : rows_) {
    if (row.overrides.empty() && !row.hideEquipped) {
      continue;
    }

    nlohmann::json serializedRow;
    if (!SerializeRowSource(row, serializedRow)) {
      continue;
    }
    if (row.conditionId.has_value()) {
      serializedRow["conditionId"] = *row.conditionId;
    } else {
      serializedRow["conditionId"] = nullptr;
    }
    serializedRow["hideEquipped"] = row.hideEquipped;
    serializedRow["overrides"] = nlohmann::json::array();
    for (const auto &overrideItem : row.overrides) {
      if (const auto *overrideForm =
              RE::TESForm::LookupByID(overrideItem.formID)) {
        serializedRow["overrides"].push_back(
            armor::GetFormIdentifier(overrideForm));
      }
    }

    root["rows"].push_back(std::move(serializedRow));
  }

  const auto payload = root.dump();
  a_skse->WriteRecord(kSerializationType, kSerializationVersion, payload.data(),
                      static_cast<std::uint32_t>(payload.size()));
}

void VariantWorkbench::Deserialize(SKSE::SerializationInterface *a_skse) {
  Revert();

  std::uint32_t type = 0;
  std::uint32_t version = 0;
  std::uint32_t length = 0;
  if (!a_skse->GetNextRecordInfo(type, version, length)) {
    return;
  }

  if (type != kSerializationType) {
    return;
  }

  if (version != 2 && version != 3 && version != kSerializationVersion) {
    logger::warn("Skipping SOSR serialized rows from unsupported version {}",
                 version);
    return;
  }

  std::string payload(length, '\0');
  if (!a_skse->ReadRecordData(payload.data(), length)) {
    logger::error("Failed to read SOSR serialized workbench payload");
    return;
  }

  const auto root = nlohmann::json::parse(payload, nullptr, false, true);
  if (root.is_discarded() || !root.is_object() || !root["rows"].is_array()) {
    logger::error("Failed to parse SOSR serialized workbench payload");
    return;
  }

  for (const auto &serializedRow : root["rows"]) {
    if (!serializedRow.is_object()) {
      continue;
    }

    VariantWorkbenchRow row{};
    if (!DeserializeRowSource(serializedRow, *this, row)) {
      continue;
    }
    if (version >= 4) {
      if (const auto conditionIt = serializedRow.find("conditionId");
          conditionIt != serializedRow.end()) {
        if (conditionIt->is_null()) {
          row.conditionId = std::nullopt;
        } else if (conditionIt->is_string()) {
          const auto conditionId = conditionIt->get<std::string>();
          row.conditionId = conditionId.empty()
                                ? std::nullopt
                                : std::optional<std::string>(conditionId);
        } else {
          row.conditionId = std::string(ui::conditions::kDefaultConditionId);
        }
      } else {
        row.conditionId = std::string(ui::conditions::kDefaultConditionId);
      }
    } else {
      row.conditionId = std::string(ui::conditions::kDefaultConditionId);
    }
    UpdateRowIdentity(row);
    row.hideEquipped = serializedRow.value("hideEquipped", false);

    std::unordered_set<RE::FormID> seenOverrideForms;
    if (const auto &serializedOverrides = serializedRow["overrides"];
        serializedOverrides.is_array()) {
      for (const auto &overrideValue : serializedOverrides) {
        if (!overrideValue.is_string()) {
          continue;
        }

        const auto *overrideForm = armor::LookupByIdentifier<RE::TESObjectARMO>(
            overrideValue.get<std::string>());
        EquipmentWidgetItem overrideItem{};
        if (!overrideForm ||
            !BuildCatalogItem(overrideForm->GetFormID(), overrideItem) ||
            !seenOverrideForms.insert(overrideForm->GetFormID()).second) {
          continue;
        }

        row.overrides.push_back(std::move(overrideItem));
      }
    }

    rows_.push_back(std::move(row));
    rowOrder_.push_back(rows_.back().key);
  }
}

void VariantWorkbench::Revert() {
  ClearPreview();

  auto *dav = GetDynamicArmorVariantsExtendedClient();
  bool variantsChanged = false;
  sosr::conditions::RefreshTargets refreshTargets;

  if (dav) {
    for (const auto &[variantName, state] : activeDavVariants_) {
      if (!dav->DeleteVariant(variantName.c_str())) {
        logger::warn("Failed to delete DAV variant {} during SOSR revert",
                     variantName);
      } else {
        sosr::conditions::MergeRefreshTargets(refreshTargets,
                                              state.refreshTargets);
        variantsChanged = true;
      }
    }
    if (variantsChanged) {
      sosr::conditions::RefreshActors(*dav, refreshTargets);
    }
  }

  rows_.clear();
  RebuildRowOrder();
  activeDavVariants_.clear();
}
} // namespace sosr::workbench
