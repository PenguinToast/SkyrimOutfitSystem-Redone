#pragma once

#include <RE/Skyrim.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace sosr::conditions {
struct MaterializationState {
  bool valid{false};
  std::shared_ptr<RE::TESCondition> condition;
  std::string signature;
  std::vector<std::uint32_t> refreshActorFormIDs;
  bool refreshUseNearbyFallback{false};

  void Clear() {
    valid = false;
    condition.reset();
    signature.clear();
    refreshActorFormIDs.clear();
    refreshUseNearbyFallback = false;
  }
};
} // namespace sosr::conditions
