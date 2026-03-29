#pragma once

#include "conditions/Definition.h"

#include <string>

namespace sosr::conditions {
[[nodiscard]] Clause BuildDefaultPlayerClause();
[[nodiscard]] Definition BuildDefaultPlayerCondition();
[[nodiscard]] std::string BuildConditionId(int a_nextConditionId);
} // namespace sosr::conditions
