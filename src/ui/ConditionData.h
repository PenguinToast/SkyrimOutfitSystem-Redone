#pragma once

#include "conditions/Definition.h"
#include "imgui.h"

namespace sosr::ui::conditions {
using Comparator = sosr::conditions::Comparator;
using Connective = sosr::conditions::Connective;
using Clause = sosr::conditions::Clause;
using Color = sosr::conditions::Color;
using Definition = sosr::conditions::Definition;

inline constexpr std::string_view kDefaultConditionId =
    sosr::conditions::kDefaultConditionId;

[[nodiscard]] inline ImVec4 ToImGuiColor(const Color &a_color) {
  return ImVec4(a_color.x, a_color.y, a_color.z, a_color.w);
}

[[nodiscard]] inline Color FromImGuiColor(const ImVec4 &a_color) {
  return Color{a_color.x, a_color.y, a_color.z, a_color.w};
}
} // namespace sosr::ui::conditions
