#pragma once

#include "ui/ConditionData.h"

#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace sosr::ui::condition_editor {
[[nodiscard]] bool
IsBooleanComparator(sosr::conditions::Comparator a_comparator);
[[nodiscard]] sosr::conditions::Color PickDistinctConditionColor(
    std::span<const sosr::conditions::Color> a_existingColors);
[[nodiscard]] std::string BuildSuggestedConditionName(
    const std::vector<sosr::conditions::Definition> &a_conditions, int a_seed,
    const std::function<bool(std::string_view)> &a_extraConflict = {});
[[nodiscard]] sosr::conditions::Definition
BuildNewConditionTemplate(const std::string &a_name,
                          const sosr::conditions::Color &a_color);
[[nodiscard]] std::string ValidateConditionDraft(
    const sosr::conditions::Definition &a_definition,
    const std::vector<sosr::conditions::Definition> &a_conditions);
[[nodiscard]] bool ParseBooleanComparand(std::string_view a_text,
                                         bool a_defaultValue);
} // namespace sosr::ui::condition_editor
