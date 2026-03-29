#include "ui/conditions/EditorSupport.h"

#include "conditions/Defaults.h"
#include "conditions/Validation.h"
#include "ui/ConditionFunctionMetadata.h"
#include "ui/ConditionParamOptionCache.h"
#include "ui/components/EditableCombo.h"

#include <RE/C/CommandTable.h>
#include <imgui.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cfloat>
#include <sstream>

namespace {
using Clause = sosr::conditions::Clause;
using ConditionColor = sosr::conditions::Color;
using Comparator = sosr::conditions::Comparator;
using Definition = sosr::conditions::Definition;
using FunctionInfo = sosr::ui::condition_editor::FunctionInfo;
using ValueEditorKind = sosr::ui::condition_editor::ValueEditorKind;

constexpr std::string_view kDropdownSectionPrefix = "\x1fsection:";

std::string BuildSectionEntry(std::string_view a_label) {
  std::string entry{kDropdownSectionPrefix};
  entry.append(a_label);
  return entry;
}

float ComputeColorDistanceSq(const ConditionColor &a_left,
                            const ConditionColor &a_right) {
  const auto dr = a_left.x - a_right.x;
  const auto dg = a_left.y - a_right.y;
  const auto db = a_left.z - a_right.z;
  return (dr * dr) + (dg * dg) + (db * db);
}

std::string FormatNumberStringImpl(const double a_value) {
  std::ostringstream stream;
  stream.setf(std::ios::fixed, std::ios::floatfield);
  stream << std::setprecision(3) << a_value;
  auto text = stream.str();
  const auto dotIndex = text.find('.');
  if (dotIndex != std::string::npos) {
    while (!text.empty() && text.back() == '0') {
      text.pop_back();
    }
    if (!text.empty() && text.back() == '.') {
      text.pop_back();
    }
  }
  return text.empty() ? "0" : text;
}

ValueEditorKind GetEditorKindForParamTypeImpl(const RE::SCRIPT_PARAM_TYPE a_type) {
  switch (a_type) {
  case RE::SCRIPT_PARAM_TYPE::kChar:
  case RE::SCRIPT_PARAM_TYPE::kInt:
  case RE::SCRIPT_PARAM_TYPE::kStage:
  case RE::SCRIPT_PARAM_TYPE::kRelationshipRank:
    return ValueEditorKind::Integer;
  case RE::SCRIPT_PARAM_TYPE::kFloat:
    return ValueEditorKind::Number;
  default:
    return sosr::ui::conditions::ConditionParamOptionCache::Supports(a_type)
               ? ValueEditorKind::CachedOption
               : ValueEditorKind::Unsupported;
  }
}

bool SupportsTypedParamInput(const RE::SCRIPT_PARAM_TYPE a_type) {
  return GetEditorKindForParamTypeImpl(a_type) != ValueEditorKind::Unsupported;
}

RE::SCRIPT_PARAM_TYPE ResolveEditorParamTypeImpl(
    std::string_view a_functionName, const std::uint16_t a_paramIndex,
    const RE::SCRIPT_PARAM_TYPE a_type) {
  if (a_paramIndex == 0 &&
      sosr::ui::condition_editor::CompareTextInsensitive(a_functionName,
                                                         "GetIsID") == 0) {
    return RE::SCRIPT_PARAM_TYPE::kActorBase;
  }

  return a_type;
}

bool SupportsTypedFunction(const RE::SCRIPT_FUNCTION &a_command) {
  if (!a_command.conditionFunction || a_command.numParams > 2 ||
      sosr::ui::conditions::IsObsoleteConditionFunction(a_command)) {
    return false;
  }

  for (std::uint16_t paramIndex = 0; paramIndex < a_command.numParams;
       ++paramIndex) {
    const auto paramType = ResolveEditorParamTypeImpl(
        a_command.functionName ? a_command.functionName : "", paramIndex,
        a_command.params ? a_command.params[paramIndex].paramType.get()
                         : RE::SCRIPT_PARAM_TYPE::kForm);
    if (!SupportsTypedParamInput(paramType)) {
      return false;
    }
  }

  return true;
}

bool DrawNumericClauseValueEditorImpl(const char *a_id, std::string &a_value,
                                      const ValueEditorKind a_kind,
                                      const float a_width) {
  if (a_kind == ValueEditorKind::Unsupported ||
      a_kind == ValueEditorKind::CachedOption) {
    ImGui::BeginDisabled();
    ImGui::SetNextItemWidth(a_width);
    char buffer[32] = "Unsupported";
    ImGui::InputText(a_id, buffer, sizeof(buffer),
                     ImGuiInputTextFlags_ReadOnly);
    ImGui::EndDisabled();
    return false;
  }

  ImGui::SetNextItemWidth(a_width);

  if (a_kind == ValueEditorKind::Integer) {
    int numericValue = 0;
    if (!a_value.empty()) {
      try {
        numericValue = std::stoi(a_value);
      } catch (const std::exception &) {
      }
    }

    if (ImGui::InputInt(a_id, &numericValue, 0, 0)) {
      a_value = std::to_string(numericValue);
      return true;
    }
    return false;
  }

  if (a_kind == ValueEditorKind::Number) {
    double numericValue = 0.0;
    if (!a_value.empty()) {
      try {
        numericValue = std::stod(a_value);
      } catch (const std::exception &) {
      }
    }

    if (ImGui::InputDouble(a_id, &numericValue, 0.0, 0.0, "%.3f")) {
      a_value = FormatNumberStringImpl(numericValue);
      return true;
    }
    return false;
  }

  return false;
}
} // namespace

namespace sosr::ui::condition_editor {
std::string TrimText(std::string_view a_text) {
  std::size_t start = 0;
  while (start < a_text.size() &&
         std::isspace(static_cast<unsigned char>(a_text[start])) != 0) {
    ++start;
  }

  std::size_t end = a_text.size();
  while (end > start &&
         std::isspace(static_cast<unsigned char>(a_text[end - 1])) != 0) {
    --end;
  }

  return std::string(a_text.substr(start, end - start));
}

int CompareTextInsensitive(std::string_view a_left, std::string_view a_right) {
  const auto leftSize = a_left.size();
  const auto rightSize = a_right.size();
  const auto count = (std::min)(leftSize, rightSize);

  for (std::size_t index = 0; index < count; ++index) {
    const auto left = static_cast<unsigned char>(
        std::tolower(static_cast<unsigned char>(a_left[index])));
    const auto right = static_cast<unsigned char>(
        std::tolower(static_cast<unsigned char>(a_right[index])));
    if (left < right) {
      return -1;
    }
    if (left > right) {
      return 1;
    }
  }

  if (leftSize < rightSize) {
    return -1;
  }
  if (leftSize > rightSize) {
    return 1;
  }
  return 0;
}

bool IsBooleanComparator(const Comparator a_comparator) {
  return a_comparator == Comparator::Equal ||
         a_comparator == Comparator::NotEqual;
}

ValueEditorKind GetEditorKindForParamType(const RE::SCRIPT_PARAM_TYPE a_type) {
  return GetEditorKindForParamTypeImpl(a_type);
}

RE::SCRIPT_PARAM_TYPE ResolveEditorParamType(std::string_view a_functionName,
                                             const std::uint16_t a_paramIndex,
                                             const RE::SCRIPT_PARAM_TYPE a_type) {
  return ResolveEditorParamTypeImpl(a_functionName, a_paramIndex, a_type);
}

std::string FormatNumberString(const double a_value) {
  return FormatNumberStringImpl(a_value);
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

  auto scoreColor = [&](const ConditionColor &a_candidate) {
    float minDistanceSq = FLT_MAX;
    bool hasExisting = false;
    for (const auto &existing : a_existingColors) {
      minDistanceSq = (std::min)(minDistanceSq,
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
  auto conflicts = [&](std::string_view a_candidate) {
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

const std::vector<FunctionInfo> &GetConditionFunctionInfos() {
  static const auto infos = [] {
    std::vector<FunctionInfo> result;

    const auto *commands = RE::SCRIPT_FUNCTION::GetFirstScriptCommand();
    if (!commands) {
      return result;
    }

    for (std::uint32_t index = 0;
         index < RE::SCRIPT_FUNCTION::Commands::kScriptCommandsEnd; ++index) {
      const auto &command = commands[index];
      if (!command.functionName || !SupportsTypedFunction(command)) {
        continue;
      }

      const auto name = TrimText(command.functionName);
      if (name.empty()) {
        continue;
      }

      if (std::ranges::find_if(result, [&](const FunctionInfo &info) {
            return CompareTextInsensitive(info.name, name) == 0;
          }) != result.end()) {
        continue;
      }

      FunctionInfo info;
      info.name = name;
      info.parameterCount = command.numParams;
      info.returnsBooleanResult =
          sosr::ui::conditions::ReturnsBooleanConditionResult(info.name);
      for (std::uint16_t paramIndex = 0;
           paramIndex < command.numParams && paramIndex < 2; ++paramIndex) {
        if (command.params && command.params[paramIndex].paramName) {
          info.parameterLabels[paramIndex] =
              TrimText(command.params[paramIndex].paramName);
        }
        if (info.parameterLabels[paramIndex].empty()) {
          info.parameterLabels[paramIndex] =
              "Argument " + std::to_string(paramIndex + 1);
        }
        info.parameterOptional[paramIndex] =
            command.params != nullptr && command.params[paramIndex].optional;
        info.parameterTypes[paramIndex] =
            command.params ? command.params[paramIndex].paramType.get()
                           : RE::SCRIPT_PARAM_TYPE::kForm;
      }

      result.push_back(std::move(info));
    }

    std::ranges::sort(result, [](const auto &left, const auto &right) {
      return CompareTextInsensitive(left.name, right.name) < 0;
    });
    return result;
  }();

  return infos;
}

const FunctionInfo *FindConditionFunctionInfo(std::string_view a_name) {
  const auto &infos = GetConditionFunctionInfos();
  const auto it = std::ranges::find_if(infos, [&](const auto &info) {
    return CompareTextInsensitive(info.name, a_name) == 0;
  });
  return it != infos.end() ? std::addressof(*it) : nullptr;
}

const FunctionInfo *ResolveConditionFunctionInfo(
    const Clause &a_clause, const std::vector<Definition> &a_conditions,
    std::optional<FunctionInfo> &a_customInfo) {
  if (!a_clause.customConditionId.empty()) {
    if (const auto *condition =
            sosr::conditions::FindDefinitionById(a_conditions,
                                                 a_clause.customConditionId);
        condition != nullptr) {
      a_customInfo = FunctionInfo{};
      a_customInfo->name = condition->name;
      a_customInfo->parameterCount = 0;
      a_customInfo->returnsBooleanResult = true;
      return std::addressof(*a_customInfo);
    }
  }

  return FindConditionFunctionInfo(a_clause.functionName);
}

std::string ResolveClauseDisplayName(
    const Clause &a_clause, const std::vector<Definition> &a_conditions) {
  if (!a_clause.customConditionId.empty()) {
    if (const auto *condition =
            sosr::conditions::FindDefinitionById(a_conditions,
                                                 a_clause.customConditionId);
        condition != nullptr) {
      return condition->name;
    }
  }
  return a_clause.functionName;
}

std::vector<std::string> BuildConditionFunctionNames(
    const std::vector<Definition> &a_conditions,
    std::string_view a_excludedConditionId) {
  std::vector<std::string> names;
  const auto &infos = GetConditionFunctionInfos();

  std::vector<std::string> customNames;
  customNames.reserve(a_conditions.size());
  for (const auto &condition : a_conditions) {
    if (condition.id == a_excludedConditionId) {
      continue;
    }
    customNames.push_back(condition.name);
  }
  std::ranges::sort(customNames, [](const auto &left, const auto &right) {
    return CompareTextInsensitive(left, right) < 0;
  });

  names.reserve(customNames.size() + infos.size() + 2);
  if (!customNames.empty()) {
    names.push_back(BuildSectionEntry("Conditions"));
    names.insert(names.end(), customNames.begin(), customNames.end());
  }
  names.push_back(BuildSectionEntry("Condition Functions"));
  for (const auto &info : infos) {
    names.push_back(info.name);
  }
  return names;
}

std::string ValidateConditionDraft(const Definition &a_definition,
                                   const std::vector<Definition> &a_conditions) {
  if (const auto baseValidation =
          sosr::conditions::ValidateDefinitionNameAndGraph(
          a_definition, a_conditions, [](std::string_view a_name) {
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

      const auto editorKind = GetEditorKindForParamTypeImpl(
          ResolveEditorParamTypeImpl(functionInfo->name, paramIndex,
                                     functionInfo->parameterTypes[paramIndex]));
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

bool DrawNumericClauseValueEditor(const char *a_id, std::string &a_value,
                                  const ValueEditorKind a_kind,
                                  const float a_width) {
  return DrawNumericClauseValueEditorImpl(a_id, a_value, a_kind, a_width);
}

bool DrawConditionParamEditor(const char *a_id, std::string &a_value,
                              const RE::SCRIPT_PARAM_TYPE a_type,
                              const float a_width) {
  const auto editorKind = GetEditorKindForParamTypeImpl(a_type);
  if (editorKind == ValueEditorKind::Integer ||
      editorKind == ValueEditorKind::Number) {
    return DrawNumericClauseValueEditorImpl(a_id, a_value, editorKind, a_width);
  }

  if (editorKind == ValueEditorKind::CachedOption) {
    auto &optionCache = sosr::ui::conditions::ConditionParamOptionCache::Get();
    const auto state = optionCache.Ensure(a_type);
    if (state == sosr::ui::conditions::ConditionParamOptionCache::State::Ready) {
      const auto *options = optionCache.GetOptions(a_type);
      if (options) {
        return sosr::ui::components::DrawSearchableDropdown(
            a_id, "Select value", a_value, *options, a_width);
      }
    } else if (state ==
               sosr::ui::conditions::ConditionParamOptionCache::State::Loading) {
      const auto progress =
          std::clamp(optionCache.GetProgress(a_type), 0.0f, 1.0f);
      const auto status = optionCache.GetStatus(a_type);
      ImGui::ProgressBar(progress, ImVec2(a_width, 0.0f),
                         status.empty() ? nullptr : status.data());
      return false;
    }
  }

  ImGui::BeginDisabled();
  ImGui::SetNextItemWidth(a_width);
  char buffer[32] = "Unsupported";
  ImGui::InputText(a_id, buffer, sizeof(buffer), ImGuiInputTextFlags_ReadOnly);
  ImGui::EndDisabled();
  return false;
}
} // namespace sosr::ui::condition_editor
