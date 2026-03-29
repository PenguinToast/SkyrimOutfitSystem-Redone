#pragma once

#include "ConditionRefreshTargets.h"
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
  RefreshTargets refreshTargets;
};

[[nodiscard]] const ui::conditions::Definition *
FindDefinitionById(const std::vector<ui::conditions::Definition> &a_conditions,
                   std::string_view a_id);

[[nodiscard]] ui::conditions::Definition *
FindDefinitionById(std::vector<ui::conditions::Definition> &a_conditions,
                   std::string_view a_id);

void RebuildConditionDependencyMetadata(
    std::vector<ui::conditions::Definition> &a_conditions);

void InvalidateConditionMaterializationCaches(
    std::vector<ui::conditions::Definition> &a_conditions);

void InvalidateConditionMaterializationCachesFrom(
    std::vector<ui::conditions::Definition> &a_conditions,
    std::string_view a_conditionId);

[[nodiscard]] std::optional<MaterializedCondition> MaterializeConditionById(
    std::string_view a_conditionId,
    std::vector<ui::conditions::Definition> &a_conditions);
} // namespace sosr::conditions
