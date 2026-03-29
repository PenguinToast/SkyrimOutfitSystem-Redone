#pragma once

#include "RE/C/CommandTable.h"

#include <string_view>

namespace sosr::ui::conditions {

[[nodiscard]] bool IsObsoleteConditionFunction(std::string_view a_name);
[[nodiscard]] bool
IsObsoleteConditionFunction(const RE::SCRIPT_FUNCTION &a_command);

[[nodiscard]] bool ReturnsBooleanConditionResult(std::string_view a_name);

} // namespace sosr::ui::conditions
