#include "ConditionRefreshTargets.h"

#include "RE/T/TESCondition.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace {
constexpr float kNearbyRefreshRadius = 2048.0f;

bool IsNearlyEqual(const float a_left, const float a_right) {
  return std::fabs(a_left - a_right) <= 0.001f;
}

bool IsPositiveGetIsReferenceCheck(const RE::CONDITION_ITEM_DATA &a_data) {
  const auto comparand = a_data.comparisonValue.f;
  switch (a_data.flags.opCode) {
  case RE::CONDITION_ITEM_DATA::OpCode::kEqualTo:
    return IsNearlyEqual(comparand, 1.0f);
  case RE::CONDITION_ITEM_DATA::OpCode::kNotEqualTo:
    return IsNearlyEqual(comparand, 0.0f);
  default:
    return false;
  }
}
} // namespace

namespace sosr::conditions {
RefreshTargets
BuildRefreshTargets(const std::shared_ptr<RE::TESCondition> &a_condition) {
  RefreshTargets targets;
  std::unordered_set<RE::FormID> seenActorFormIDs;

  if (!a_condition) {
    targets.useNearbyFallback = true;
    return targets;
  }

  for (auto *item = a_condition->head; item != nullptr; item = item->next) {
    if (item->data.functionData.function !=
            RE::FUNCTION_DATA::FunctionID::kGetIsReference ||
        !IsPositiveGetIsReferenceCheck(item->data)) {
      continue;
    }

    auto *ref =
        static_cast<RE::TESObjectREFR *>(item->data.functionData.params[0]);
    auto *actor = ref ? ref->As<RE::Actor>() : nullptr;
    if (!actor) {
      continue;
    }

    if (seenActorFormIDs.insert(actor->GetFormID()).second) {
      targets.actorFormIDs.push_back(actor->GetFormID());
    }
  }

  if (targets.actorFormIDs.empty()) {
    targets.useNearbyFallback = true;
  }

  return targets;
}

void MergeRefreshTargets(RefreshTargets &a_target,
                         const RefreshTargets &a_source) {
  a_target.useNearbyFallback =
      a_target.useNearbyFallback || a_source.useNearbyFallback;

  if (a_source.actorFormIDs.empty()) {
    return;
  }

  std::unordered_set<RE::FormID> seenActorFormIDs(a_target.actorFormIDs.begin(),
                                                  a_target.actorFormIDs.end());
  for (const auto actorFormID : a_source.actorFormIDs) {
    if (seenActorFormIDs.insert(actorFormID).second) {
      a_target.actorFormIDs.push_back(actorFormID);
    }
  }
}

bool RefreshActors(
    DynamicArmorVariantsExtendedAPI::IDynamicArmorVariantsExtendedInterface001
        &a_dav,
    const RefreshTargets &a_targets) {
  auto *player = RE::PlayerCharacter::GetSingleton();
  if (!player) {
    return false;
  }

  if (!a_targets.useNearbyFallback && !a_targets.actorFormIDs.empty()) {
    bool refreshedAny = false;
    for (const auto actorFormID : a_targets.actorFormIDs) {
      auto *actor = RE::TESForm::LookupByID<RE::Actor>(actorFormID);
      if (!actor) {
        continue;
      }

      refreshedAny = a_dav.RefreshActor(actor) || refreshedAny;
    }
    return refreshedAny;
  }

  bool refreshedAny = a_dav.RefreshActor(player);
  std::unordered_set<RE::FormID> refreshedActorFormIDs{player->GetFormID()};

  if (auto *tes = RE::TES::GetSingleton(); tes != nullptr) {
    tes->ForEachReferenceInRange(
        player, kNearbyRefreshRadius, [&](RE::TESObjectREFR *a_ref) {
          auto *actor = a_ref ? a_ref->As<RE::Actor>() : nullptr;
          if (!actor ||
              !refreshedActorFormIDs.insert(actor->GetFormID()).second) {
            return RE::BSContainer::ForEachResult::kContinue;
          }

          refreshedAny = a_dav.RefreshActor(actor) || refreshedAny;
          return RE::BSContainer::ForEachResult::kContinue;
        });
  }

  return refreshedAny;
}
} // namespace sosr::conditions
