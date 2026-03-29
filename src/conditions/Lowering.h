#pragma once

#include "conditions/Definition.h"

#include <RE/Skyrim.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace sosr::conditions {
struct LoweredMaterialization {
  std::shared_ptr<RE::TESCondition> condition;
  std::string signature;
};

[[nodiscard]] std::optional<LoweredMaterialization>
LowerAndEmitCondition(const Definition &a_definition,
                      const std::vector<Definition> &a_conditions);
} // namespace sosr::conditions
