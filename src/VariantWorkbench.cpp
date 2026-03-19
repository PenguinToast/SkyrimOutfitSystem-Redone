#include "VariantWorkbench.h"

#include "ArmorUtils.h"
#include "integrations/DynamicArmorVariantsClient.h"

#include <nlohmann/json.hpp>
#include <unordered_set>

namespace
{
    std::string BuildDavVariantName(const RE::TESObjectARMO* a_sourceArmor)
    {
        return "SOSNG|" + sosng::armor::GetFormIdentifier(a_sourceArmor);
    }

    auto BuildDavConditionsJson() -> std::string
    {
        const nlohmann::json root{
            { "conditions", nlohmann::json::array({ "GetIsReference Player == 1" }) }
        };
        return root.dump();
    }

    auto BuildDavVariantJson(const RE::TESObjectARMO* a_sourceArmor, const std::vector<const RE::TESObjectARMO*>& a_overrideArmors) -> std::string
    {
        nlohmann::json replaceByForm = nlohmann::json::object();
        std::vector<std::pair<std::uint64_t, std::string>> overrideAddons;
        std::string displayName;

        for (const auto* overrideArmor : a_overrideArmors) {
            if (!overrideArmor) {
                continue;
            }

            if (!displayName.empty()) {
                displayName.append(" + ");
            }
            displayName.append(sosng::armor::GetDisplayName(overrideArmor));

            for (const auto* overrideAddon : overrideArmor->armorAddons) {
                if (!overrideAddon) {
                    continue;
                }

                const auto identifier = sosng::armor::GetFormIdentifier(overrideAddon);
                if (identifier.empty()) {
                    continue;
                }

                overrideAddons.emplace_back(overrideAddon->bipedModelData.bipedObjectSlots.underlying(), identifier);
            }
        }

        const RE::TESObjectARMA* sourceAddon = nullptr;
        for (const auto* candidate : a_sourceArmor->armorAddons) {
            if (candidate) {
                sourceAddon = candidate;
                break;
            }
        }

        if (sourceAddon) {
            const auto sourceIdentifier = sosng::armor::GetFormIdentifier(sourceAddon);
            if (!sourceIdentifier.empty()) {
                std::vector<std::string> replacements;
                std::unordered_set<std::string> replacementSet;

                for (const auto& [_, identifier] : overrideAddons) {
                    if (replacementSet.insert(identifier).second) {
                        replacements.push_back(identifier);
                    }
                }

                if (!replacements.empty()) {
                    if (replacements.size() == 1) {
                        replaceByForm[sourceIdentifier] = replacements.front();
                    } else {
                        replaceByForm[sourceIdentifier] = replacements;
                    }
                }
            }
        }

        if (replaceByForm.empty()) {
            return {};
        }

        nlohmann::json root{
            { "displayName", displayName.empty() ? sosng::armor::GetDisplayName(a_sourceArmor) : displayName },
            { "replaceByForm", replaceByForm }
        };
        return root.dump();
    }
}

namespace sosng::workbench
{
    void VariantWorkbench::SyncRowsFromPlayer()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            rows_.clear();
            rowOrder_.clear();
            return;
        }

        std::unordered_map<std::string, std::vector<EquipmentWidgetItem>> existingOverrides;
        existingOverrides.reserve(rows_.size());
        for (auto& row : rows_) {
            existingOverrides.emplace(row.key, std::move(row.overrides));
        }

        std::unordered_map<std::string, VariantWorkbenchRow> rowMap;
        rowMap.reserve(rows_.size() + 8);
        std::vector<std::string> discoveredKeys;
        discoveredKeys.reserve(rows_.size() + 8);
        std::unordered_set<RE::FormID> seenArmorForms;

        for (const auto slot : { RE::BGSBipedObjectForm::BipedObjectSlot::kHead,
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
                                 RE::BGSBipedObjectForm::BipedObjectSlot::kFX01 }) {
            auto* armor = player->GetWornArmor(slot);
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

            VariantWorkbenchRow row{};
            row.key = "armor:" + armor::FormatFormID(formID);
            row.equipped = std::move(equipped);
            row.equipped.key = row.key;
            if (const auto it = existingOverrides.find(row.key); it != existingOverrides.end()) {
                row.overrides = std::move(it->second);
            }
            discoveredKeys.push_back(row.key);
            rowMap.emplace(row.key, std::move(row));
        }

        std::vector<VariantWorkbenchRow> rows;
        rows.reserve(rowMap.size());
        std::unordered_set<std::string> placedKeys;
        placedKeys.reserve(rowMap.size());

        for (const auto& key : rowOrder_) {
            const auto it = rowMap.find(key);
            if (it == rowMap.end()) {
                continue;
            }

            placedKeys.insert(key);
            rows.push_back(std::move(it->second));
        }

        for (const auto& key : discoveredKeys) {
            if (!placedKeys.insert(key).second) {
                continue;
            }

            const auto it = rowMap.find(key);
            if (it == rowMap.end()) {
                continue;
            }

            rows.push_back(std::move(it->second));
        }

        rows_ = std::move(rows);
        rowOrder_.clear();
        rowOrder_.reserve(rows_.size());
        for (const auto& row : rows_) {
            rowOrder_.push_back(row.key);
        }
    }

    bool VariantWorkbench::BuildCatalogItem(RE::FormID a_formID, EquipmentWidgetItem& a_item) const
    {
        const auto* form = RE::TESForm::LookupByID(a_formID);
        if (!form) {
            return false;
        }

        a_item = {};
        a_item.formID = form->GetFormID();
        a_item.key = "form:" + armor::FormatFormID(form->GetFormID());
        a_item.name = armor::GetDisplayName(form);

        if (const auto* armorForm = form->As<RE::TESObjectARMO>()) {
            a_item.slotMask = armorForm->GetSlotMask().underlying();
            a_item.slotText = armor::JoinStrings(armor::GetArmorSlotLabels(a_item.slotMask));
            return true;
        }

        return false;
    }

    bool VariantWorkbench::CanAcceptOverride(int a_targetRowIndex, const EquipmentWidgetItem& a_item, int a_sourceRowIndex, int a_sourceItemIndex) const
    {
        if (a_targetRowIndex < 0 || a_targetRowIndex >= static_cast<int>(rows_.size())) {
            return false;
        }

        if (a_sourceRowIndex == a_targetRowIndex && a_sourceItemIndex >= 0) {
            return false;
        }

        const auto& row = rows_[static_cast<std::size_t>(a_targetRowIndex)];
        if (a_item.slotMask == 0) {
            return false;
        }

        for (int itemIndex = 0; itemIndex < static_cast<int>(row.overrides.size()); ++itemIndex) {
            if (a_targetRowIndex == a_sourceRowIndex && itemIndex == a_sourceItemIndex) {
                continue;
            }

            const auto& existing = row.overrides[static_cast<std::size_t>(itemIndex)];
            if ((existing.slotMask & a_item.slotMask) != 0) {
                return false;
            }
        }

        return true;
    }

    bool VariantWorkbench::AddCatalogOverride(int a_targetRowIndex, RE::FormID a_formID)
    {
        EquipmentWidgetItem item{};
        if (!BuildCatalogItem(a_formID, item) || !CanAcceptOverride(a_targetRowIndex, item)) {
            return false;
        }

        rows_[static_cast<std::size_t>(a_targetRowIndex)].overrides.push_back(std::move(item));
        return true;
    }

    bool VariantWorkbench::MoveOverride(int a_sourceRowIndex, int a_sourceItemIndex, int a_targetRowIndex)
    {
        if (a_sourceRowIndex < 0 || a_sourceRowIndex >= static_cast<int>(rows_.size())) {
            return false;
        }

        auto& sourceOverrides = rows_[static_cast<std::size_t>(a_sourceRowIndex)].overrides;
        if (a_sourceItemIndex < 0 || a_sourceItemIndex >= static_cast<int>(sourceOverrides.size())) {
            return false;
        }

        auto item = sourceOverrides[static_cast<std::size_t>(a_sourceItemIndex)];
        if (!CanAcceptOverride(a_targetRowIndex, item, a_sourceRowIndex, a_sourceItemIndex)) {
            return false;
        }

        sourceOverrides.erase(sourceOverrides.begin() + a_sourceItemIndex);
        rows_[static_cast<std::size_t>(a_targetRowIndex)].overrides.push_back(std::move(item));
        return true;
    }

    bool VariantWorkbench::DeleteOverride(int a_rowIndex, int a_itemIndex)
    {
        if (a_rowIndex < 0 || a_rowIndex >= static_cast<int>(rows_.size())) {
            return false;
        }

        auto& overrides = rows_[static_cast<std::size_t>(a_rowIndex)].overrides;
        if (a_itemIndex < 0 || a_itemIndex >= static_cast<int>(overrides.size())) {
            return false;
        }

        overrides.erase(overrides.begin() + a_itemIndex);
        return true;
    }

    bool VariantWorkbench::ApplyRowReorder(int a_sourceRowIndex, int a_targetRowIndex, bool a_insertAfter)
    {
        if (a_sourceRowIndex < 0 || a_sourceRowIndex >= static_cast<int>(rows_.size())) {
            return false;
        }
        if (a_targetRowIndex < 0 || a_targetRowIndex >= static_cast<int>(rows_.size())) {
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

        rowOrder_.clear();
        rowOrder_.reserve(rows_.size());
        for (const auto& row : rows_) {
            rowOrder_.push_back(row.key);
        }

        return true;
    }

    void VariantWorkbench::SyncDynamicArmorVariants()
    {
        using integrations::DynamicArmorVariantsClient;

        auto* dav = DynamicArmorVariantsClient::Get();
        if (!dav || !dav->IsReady()) {
            return;
        }

        const auto conditionsJson = BuildDavConditionsJson();

        struct DavVariantPayload
        {
            std::string variantJson;
        };

        std::unordered_map<std::string, DavVariantPayload> desiredVariants;
        desiredVariants.reserve(rows_.size());

        for (const auto& row : rows_) {
            const auto* sourceArmor = RE::TESForm::LookupByID<RE::TESObjectARMO>(row.equipped.formID);
            if (!sourceArmor) {
                continue;
            }

            std::vector<const RE::TESObjectARMO*> overrideArmors;
            overrideArmors.reserve(row.overrides.size());
            for (const auto& overrideItem : row.overrides) {
                if (const auto* overrideArmor = RE::TESForm::LookupByID<RE::TESObjectARMO>(overrideItem.formID)) {
                    overrideArmors.push_back(overrideArmor);
                }
            }

            if (overrideArmors.empty()) {
                continue;
            }

            const auto variantName = BuildDavVariantName(sourceArmor);
            if (variantName.empty()) {
                continue;
            }

            const auto variantJson = BuildDavVariantJson(sourceArmor, overrideArmors);
            if (variantJson.empty()) {
                continue;
            }

            desiredVariants.insert_or_assign(variantName, DavVariantPayload{ std::move(variantJson) });
        }

        std::unordered_map<std::string, std::string> syncedVariants;
        syncedVariants.reserve(desiredVariants.size());
        bool variantsChanged = false;

        for (const auto& [variantName, payload] : desiredVariants) {
            if (const auto activeIt = activeDavVariants_.find(variantName);
                activeIt != activeDavVariants_.end() && activeIt->second == payload.variantJson) {
                syncedVariants.emplace(variantName, payload.variantJson);
                continue;
            }

            if (!dav->RegisterVariantJson(variantName.c_str(), payload.variantJson.c_str())) {
                logger::warn("Failed to register DAV variant {}", variantName);
                continue;
            }

            if (!dav->SetVariantConditionsJson(variantName.c_str(), conditionsJson.c_str())) {
                logger::warn("Failed to set DAV conditions for {}", variantName);
                dav->DeleteVariant(variantName.c_str());
                continue;
            }

            syncedVariants.emplace(variantName, payload.variantJson);
            variantsChanged = true;
        }

        for (const auto& [variantName, _] : activeDavVariants_) {
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
            if (auto* player = RE::PlayerCharacter::GetSingleton()) {
                dav->RefreshActor(player);
            }
        }
    }
}
