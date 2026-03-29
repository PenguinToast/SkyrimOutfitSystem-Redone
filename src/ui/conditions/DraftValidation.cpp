#include "ui/conditions/DraftValidation.h"

#include "conditions/Defaults.h"
#include "conditions/Validation.h"
#include "ui/conditions/FunctionRegistry.h"
#include "ui/conditions/ValueEditors.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cfloat>

namespace {
using Clause = sosr::conditions::Clause;
using Comparator = sosr::conditions::Comparator;
using ConditionColor = sosr::conditions::Color;
using Definition = sosr::conditions::Definition;
using FunctionInfo = sosr::ui::condition_editor::FunctionInfo;
using ValueEditorKind = sosr::ui::condition_editor::ValueEditorKind;

float ComputeColorDistanceSq(const ConditionColor &a_left,
                             const ConditionColor &a_right) {
  const auto dr = a_left.x - a_right.x;
  const auto dg = a_left.y - a_right.y;
  const auto db = a_left.z - a_right.z;
  return (dr * dr) + (dg * dg) + (db * db);
}
} // namespace

namespace sosr::ui::condition_editor {
bool IsBooleanComparator(const Comparator a_comparator) {
  return a_comparator == Comparator::Equal ||
         a_comparator == Comparator::NotEqual;
}

ConditionColor
PickDistinctConditionColor(std::span<const ConditionColor> a_existingColors) {
  static const std::array<ConditionColor, 10> kPalette{
      ConditionColor{0.86f, 0.25f, 0.28f, 1.0f},
      ConditionColor{0.17f, 0.62f, 0.32f, 1.0f},
      ConditionColor{0.19f, 0.48f, 0.85f, 1.0f},
      ConditionColor{0.86f, 0.58f, 0.16f, 1.0f},
      ConditionColor{0.55f, 0.30f, 0.86f, 1.0f},
      ConditionColor{0.10f, 0.67f, 0.67f, 1.0f},
      ConditionColor{0.84f, 0.35f, 0.63f, 1.0f},
      ConditionColor{0.61f, 0.50f, 0.18f, 1.0f},
      ConditionColor{0.24f, 0.71f, 0.86f, 1.0f},
      ConditionColor{0.93f, 0.40f, 0.13f, 1.0f}};

  const auto scoreColor = [&](const ConditionColor &a_candidate) {
    float minDistanceSq = FLT_MAX;
    bool hasExisting = false;
    for (const auto &existing : a_existingColors) {
      minDistanceSq =
          (std::min)(minDistanceSq,
                     ComputeColorDistanceSq(a_candidate, existing));
      hasExisting = true;
    }
    return hasExisting ? minDistanceSq : FLT_MAX;
  };

  ConditionColor bestColor = kPalette.front();
  float bestScore = -1.0f;
  for (const auto &candidate : kPalette) {
    const float score = scoreColor(candidate);
    if (score > bestScore) {
      bestScore = score;
      bestColor = candidate;
    }
  }

  for (int index = 0; index < 24; ++index) {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    ImGui::ColorConvertHSVtoRGB(index / 24.0f, 0.70f, 0.88f, r, g, b);
    const ConditionColor candidate{r, g, b, 1.0f};
    const float score = scoreColor(candidate);
    if (score > bestScore) {
      bestScore = score;
      bestColor = candidate;
    }
  }

  return bestColor;
}

std::string BuildSuggestedConditionName(
    const std::vector<Definition> &a_conditions, const int a_seed,
    const std::function<bool(std::string_view)> &a_extraConflict) {
  const auto conflicts = [&](std::string_view a_candidate) {
    if (FindConditionFunctionInfo(a_candidate) != nullptr ||
        sosr::conditions::FindDefinitionByName(a_conditions, a_candidate) !=
            nullptr) {
      return true;
    }

    return a_extraConflict && a_extraConflict(a_candidate);
  };

  for (int index = (std::max)(a_seed, 1);; ++index) {
    const auto candidate = "Condition " + std::to_string(index);
    if (!conflicts(candidate)) {
      return candidate;
    }
  }
}

Definition BuildNewConditionTemplate(const std::string &a_name,
                                     const ConditionColor &a_color) {
  Definition definition;
  definition.name = a_name;
  definition.color = a_color;
  definition.clauses.push_back(sosr::conditions::BuildDefaultPlayerClause());
  return definition;
}

std::string ValidateConditionDraft(const Definition &a_definition,
                                   const std::vector<Definition> &a_conditions) {
  if (const auto baseValidation =
          sosr::conditions::ValidateDefinitionNameAndGraph(
              a_definition, a_conditions,
              [](std::string_view a_name) {
                return FindConditionFunctionInfo(a_name) != nullptr;
              });
      !baseValidation.empty()) {
    return baseValidation;
  }

  for (std::size_t index = 0; index < a_definition.clauses.size(); ++index) {
    const auto &clause = a_definition.clauses[index];
    std::optional<FunctionInfo> customFunctionInfo;
    const auto *functionInfo =
        ResolveConditionFunctionInfo(clause, a_conditions, customFunctionInfo);
    if (!functionInfo) {
      return "Unknown or unsupported condition function in clause " +
             std::to_string(index + 1) + ".";
    }
    if (!clause.customConditionId.empty() &&
        clause.customConditionId == a_definition.id &&
        !a_definition.id.empty()) {
      return "A condition cannot reference itself in clause " +
             std::to_string(index + 1) + ".";
    }

    for (std::uint16_t paramIndex = 0;
         paramIndex < functionInfo->parameterCount && paramIndex < 2;
         ++paramIndex) {
      const auto argument = TrimText(clause.arguments[paramIndex]);
      if (!functionInfo->parameterOptional[paramIndex] && argument.empty()) {
        return functionInfo->parameterLabels[paramIndex] +
               " is required in clause " + std::to_string(index + 1) + ".";
      }

      const auto editorKind = GetEditorKindForParamType(ResolveEditorParamType(
          functionInfo->name, paramIndex, functionInfo->parameterTypes[paramIndex]));
      if (editorKind == ValueEditorKind::Unsupported) {
        return functionInfo->parameterLabels[paramIndex] +
               " uses an unsupported parameter type in clause " +
               std::to_string(index + 1) + ".";
      }

      if ((editorKind == ValueEditorKind::Integer ||
           editorKind == ValueEditorKind::Number) &&
          !argument.empty()) {
        try {
          if (editorKind == ValueEditorKind::Integer) {
            (void)std::stoi(argument);
          } else {
            (void)std::stod(argument);
          }
        } catch (const std::exception &) {
          return functionInfo->parameterLabels[paramIndex] +
                 (editorKind == ValueEditorKind::Integer
                      ? " must be an integer in clause "
                      : " must be numeric in clause ") +
                 std::to_string(index + 1) + ".";
        }
      }
    }

    const auto comparand = TrimText(clause.comparand);
    if (functionInfo->returnsBooleanResult) {
      if (!IsBooleanComparator(clause.comparator)) {
        return "Boolean-return conditions only support == and != in clause " +
               std::to_string(index + 1) + ".";
      }
      if (!comparand.empty() && comparand != "0" && comparand != "1") {
        return "Boolean comparison value must be 0 or 1 in clause " +
               std::to_string(index + 1) + ".";
      }
    } else {
      if (comparand.empty()) {
        return "A comparison value is required in clause " +
               std::to_string(index + 1) + ".";
      }

      try {
        (void)std::stod(comparand);
      } catch (const std::exception &) {
        return "Comparison value must be numeric in clause " +
               std::to_string(index + 1) + ".";
      }
    }
  }

  return {};
}

bool ParseBooleanComparand(std::string_view a_text, bool a_defaultValue) {
  const auto trimmed = TrimText(a_text);
  if (trimmed.empty()) {
    return a_defaultValue;
  }

  try {
    return std::stod(trimmed) != 0.0;
  } catch (const std::exception &) {
    return a_defaultValue;
  }
}
} // namespace sosr::ui::condition_editor
