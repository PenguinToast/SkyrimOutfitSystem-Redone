#include "VariantWorkbench.h"

#include "ArmorUtils.h"
#include "integrations/DynamicArmorVariantsClient.h"

#include <nlohmann/json.hpp>
#include <unordered_set>

namespace {
constexpr std::uint32_t kSerializationType = 'ROWS';
constexpr std::uint32_t kSerializationVersion = 2;

auto GetDynamicArmorVariantsClient()
    -> DynamicArmorVariantsAPI::IDynamicArmorVariantsInterface001 * {
  auto *dav = sosng::integrations::DynamicArmorVariantsClient::Get();
  if (dav) {
    return dav;
  }

  sosng::integrations::DynamicArmorVariantsClient::Refresh();
  return sosng::integrations::DynamicArmorVariantsClient::Get();
}

std::string BuildDavVariantName(const RE::TESObjectARMO *a_sourceArmor) {
  return "SOSNG|" + sosng::armor::GetFormIdentifier(a_sourceArmor);
}

std::string BuildPreviewVariantName(const std::string &a_rowKey) {
  return "SOSNG|PreviewSelected|" + a_rowKey;
}

auto BuildDavConditionsJson() -> std::string {
  const nlohmann::json root{
      {"conditions", nlohmann::json::array({"GetIsReference Player == 1"})}};
  return root.dump();
}

auto BuildDavVariantJson(
    const RE::TESObjectARMO *a_sourceArmor,
    const std::vector<const RE::TESObjectARMO *> &a_overrideArmors,
    bool a_hideEquipped) -> std::string {
  nlohmann::json replaceByForm = nlohmann::json::object();
  std::vector<std::string> replacements;
  std::unordered_set<std::string> replacementSet;
  std::string displayName;

  for (const auto *overrideArmor : a_overrideArmors) {
    if (!overrideArmor) {
      continue;
    }

    if (!displayName.empty()) {
      displayName.append(" + ");
    }
    displayName.append(sosng::armor::GetDisplayName(overrideArmor));

    for (const auto *overrideAddon : overrideArmor->armorAddons) {
      if (!overrideAddon) {
        continue;
      }

      const auto identifier =
          sosng::armor::GetReplacementIdentifier(overrideArmor, overrideAddon);
      if (identifier.empty()) {
        continue;
      }

      if (replacementSet.insert(identifier).second) {
        replacements.push_back(identifier);
      }
    }
  }

  if (replacements.empty() && !a_hideEquipped) {
    return {};
  }

  for (const auto *sourceAddon : a_sourceArmor->armorAddons) {
    if (!sourceAddon) {
      continue;
    }

    const auto sourceIdentifier = sosng::armor::GetFormIdentifier(sourceAddon);
    if (sourceIdentifier.empty()) {
      continue;
    }

    if (a_hideEquipped) {
      replaceByForm[sourceIdentifier] = nlohmann::json::array();
    } else if (replacements.size() == 1) {
      replaceByForm[sourceIdentifier] = replacements.front();
    } else {
      replaceByForm[sourceIdentifier] = replacements;
    }
  }

  if (replaceByForm.empty()) {
    return {};
  }

  nlohmann::json root{
      {"displayName",
       a_hideEquipped
           ? "Hidden " + sosng::armor::GetDisplayName(a_sourceArmor)
           : (displayName.empty() ? sosng::armor::GetDisplayName(a_sourceArmor)
                                  : displayName)},
      {"replaceByForm", replaceByForm}};
  return root.dump();
}
} // namespace

namespace sosng::workbench {
bool VariantWorkbench::ApplyCatalogPreview(RE::FormID a_formID) {
  std::vector<PlannedCatalogAssignment> assignments;
  if (!PlanCatalogAssignments(a_formID, assignments)) {
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

    const auto *sourceArmor =
        RE::TESForm::LookupByID<RE::TESObjectARMO>(rowIt->equipped.formID);
    if (!sourceArmor) {
      continue;
    }

    const auto variantJson =
        BuildDavVariantJson(sourceArmor, overrideArmors, false);
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

  if (previewFormID_ == a_formID &&
      previewDavVariants_ == desiredPreviewVariants) {
    return true;
  }

  auto *dav = GetDynamicArmorVariantsClient();
  if (!dav || !dav->IsReady()) {
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
      logger::warn("Failed to register SOSNG preview variant {}", variantName);
      for (const auto &[appliedVariantName, _] : appliedPreviewVariants) {
        dav->RemoveVariantOverride(player, appliedVariantName.c_str());
        dav->DeleteVariant(appliedVariantName.c_str());
      }
      ClearPreview();
      return false;
    }

    if (!dav->ApplyVariantOverride(player, variantName.c_str())) {
      logger::warn("Failed to apply SOSNG preview variant override {}",
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

  previewFormID_ = a_formID;
  previewDavVariants_ = std::move(desiredPreviewVariants);
  return true;
}

void VariantWorkbench::ClearPreview() {
  if (previewFormID_ == 0 && previewDavVariants_.empty()) {
    return;
  }

  auto *dav = GetDynamicArmorVariantsClient();

  if (dav) {
    if (auto *player = RE::PlayerCharacter::GetSingleton()) {
      for (const auto &[variantName, _] : previewDavVariants_) {
        if (!dav->RemoveVariantOverride(player, variantName.c_str())) {
          logger::warn("Failed to remove SOSNG preview variant override {}",
                       variantName);
        }
      }
    }
    for (const auto &[variantName, _] : previewDavVariants_) {
      if (!dav->DeleteVariant(variantName.c_str())) {
        logger::warn("Failed to delete SOSNG preview variant {}", variantName);
      }
    }
  }

  previewFormID_ = 0;
  previewDavVariants_.clear();
}

void VariantWorkbench::SyncDynamicArmorVariants() {
  auto *dav = GetDynamicArmorVariantsClient();
  if (!dav || !dav->IsReady()) {
    return;
  }

  const auto conditionsJson = BuildDavConditionsJson();

  struct DavVariantPayload {
    std::string variantJson;
  };

  std::unordered_map<std::string, DavVariantPayload> desiredVariants;
  desiredVariants.reserve(rows_.size());

  for (const auto &row : rows_) {
    const auto *sourceArmor =
        RE::TESForm::LookupByID<RE::TESObjectARMO>(row.equipped.formID);
    if (!sourceArmor) {
      continue;
    }

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

    const auto variantName = BuildDavVariantName(sourceArmor);
    if (variantName.empty()) {
      continue;
    }

    const auto variantJson =
        BuildDavVariantJson(sourceArmor, overrideArmors, row.hideEquipped);
    if (variantJson.empty()) {
      continue;
    }

    desiredVariants.insert_or_assign(variantName,
                                     DavVariantPayload{std::move(variantJson)});
  }

  std::unordered_map<std::string, std::string> syncedVariants;
  syncedVariants.reserve(desiredVariants.size());
  bool variantsChanged = false;

  for (const auto &[variantName, payload] : desiredVariants) {
    if (const auto activeIt = activeDavVariants_.find(variantName);
        activeIt != activeDavVariants_.end() &&
        activeIt->second == payload.variantJson) {
      syncedVariants.emplace(variantName, payload.variantJson);
      continue;
    }

    if (!dav->RegisterVariantJson(variantName.c_str(),
                                  payload.variantJson.c_str())) {
      logger::warn("Failed to register DAV variant {}", variantName);
      continue;
    }

    if (!dav->SetVariantConditionsJson(variantName.c_str(),
                                       conditionsJson.c_str())) {
      logger::warn("Failed to set DAV conditions for {}", variantName);
      dav->DeleteVariant(variantName.c_str());
      continue;
    }

    syncedVariants.emplace(variantName, payload.variantJson);
    variantsChanged = true;
  }

  for (const auto &[variantName, _] : activeDavVariants_) {
    if (syncedVariants.contains(variantName)) {
      continue;
    }

    if (!dav->DeleteVariant(variantName.c_str())) {
      logger::warn("Failed to delete DAV variant {}", variantName);
    } else {
      variantsChanged = true;
    }
  }

  activeDavVariants_ = std::move(syncedVariants);

  if (variantsChanged) {
    if (auto *player = RE::PlayerCharacter::GetSingleton()) {
      dav->RefreshActor(player);
    }
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
    serializedRow["equipped"] =
        armor::GetFormIdentifier(RE::TESForm::LookupByID(row.equipped.formID));
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

  if (version != kSerializationVersion) {
    logger::warn("Skipping SOSNG serialized rows from unsupported version {}",
                 version);
    return;
  }

  std::string payload(length, '\0');
  if (!a_skse->ReadRecordData(payload.data(), length)) {
    logger::error("Failed to read SOSNG serialized workbench payload");
    return;
  }

  const auto root = nlohmann::json::parse(payload, nullptr, false, true);
  if (root.is_discarded() || !root.is_object() || !root["rows"].is_array()) {
    logger::error("Failed to parse SOSNG serialized workbench payload");
    return;
  }

  for (const auto &serializedRow : root["rows"]) {
    if (!serializedRow.is_object()) {
      continue;
    }

    const auto *equippedForm = armor::LookupByIdentifier<RE::TESObjectARMO>(
        serializedRow.value("equipped", std::string{}));
    EquipmentWidgetItem equipped{};
    if (!equippedForm ||
        !BuildCatalogItem(equippedForm->GetFormID(), equipped)) {
      continue;
    }

    VariantWorkbenchRow row{};
    row.key = "armor:" + armor::FormatFormID(equippedForm->GetFormID());
    row.equipped = std::move(equipped);
    row.equipped.key = row.key;
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

  SyncDynamicArmorVariants();
}

void VariantWorkbench::Revert() {
  ClearPreview();

  auto *dav = GetDynamicArmorVariantsClient();

  if (dav) {
    for (const auto &[variantName, _] : activeDavVariants_) {
      if (!dav->DeleteVariant(variantName.c_str())) {
        logger::warn("Failed to delete DAV variant {} during SOSNG revert",
                     variantName);
      }
    }
  }

  rows_.clear();
  RebuildRowOrder();
  activeDavVariants_.clear();
}
} // namespace sosng::workbench
