#pragma once

#include "integrations/DynamicArmorVariantsExtendedAPI.h"

#include <RE/Skyrim.h>

#include <memory>
#include <vector>

namespace sosr::conditions {
struct RefreshTargets {
  std::vector<RE::FormID> actorFormIDs;
  bool useNearbyFallback{false};
};

[[nodiscard]] RefreshTargets
BuildRefreshTargets(const std::shared_ptr<RE::TESCondition> &a_condition);

void MergeRefreshTargets(RefreshTargets &a_target,
                         const RefreshTargets &a_source);

bool RefreshActors(
    DynamicArmorVariantsExtendedAPI::IDynamicArmorVariantsExtendedInterface001
        &a_dav,
    const RefreshTargets &a_targets);
} // namespace sosr::conditions
