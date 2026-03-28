#pragma once

#include "ui/ConditionData.h"

#include <RE/Skyrim.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sosr::conditions {
struct MaterializedCondition {
  std::shared_ptr<RE::TESCondition> condition;
  std::string signature;
};

[[nodiscard]] const ui::conditions::Definition *
FindDefinitionById(const std::vector<ui::conditions::Definition> &a_conditions,
                   std::string_view a_id);

[[nodiscard]] std::optional<MaterializedCondition> MaterializeConditionById(
    std::string_view a_conditionId,
    const std::vector<ui::conditions::Definition> &a_conditions);
} // namespace sosr::conditions
