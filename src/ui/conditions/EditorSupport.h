#pragma once

#include "ui/ConditionData.h"

#include <RE/Skyrim.h>

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace sosr::ui::condition_editor {
enum class ValueEditorKind : std::uint8_t {
  Unsupported,
  Integer,
  Number,
  CachedOption
};

struct FunctionInfo {
  std::string name;
  std::array<std::string, 2> parameterLabels{};
  std::array<bool, 2> parameterOptional{false, false};
  std::array<RE::SCRIPT_PARAM_TYPE, 2> parameterTypes{
      RE::SCRIPT_PARAM_TYPE::kForm, RE::SCRIPT_PARAM_TYPE::kForm};
  std::uint16_t parameterCount{0};
  bool returnsBooleanResult{false};
};

[[nodiscard]] std::string TrimText(std::string_view a_text);
[[nodiscard]] int CompareTextInsensitive(std::string_view a_left,
                                         std::string_view a_right);
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

[[nodiscard]] const std::vector<FunctionInfo> &GetConditionFunctionInfos();
[[nodiscard]] const FunctionInfo *
FindConditionFunctionInfo(std::string_view a_name);
[[nodiscard]] const FunctionInfo *ResolveConditionFunctionInfo(
    const sosr::conditions::Clause &a_clause,
    const std::vector<sosr::conditions::Definition> &a_conditions,
    std::optional<FunctionInfo> &a_customInfo);
[[nodiscard]] std::string ResolveClauseDisplayName(
    const sosr::conditions::Clause &a_clause,
    const std::vector<sosr::conditions::Definition> &a_conditions);
[[nodiscard]] std::vector<std::string> BuildConditionFunctionNames(
    const std::vector<sosr::conditions::Definition> &a_conditions,
    std::string_view a_excludedConditionId);
[[nodiscard]] std::string ValidateConditionDraft(
    const sosr::conditions::Definition &a_definition,
    const std::vector<sosr::conditions::Definition> &a_conditions);
[[nodiscard]] ValueEditorKind
GetEditorKindForParamType(RE::SCRIPT_PARAM_TYPE a_type);
[[nodiscard]] RE::SCRIPT_PARAM_TYPE
ResolveEditorParamType(std::string_view a_functionName,
                       std::uint16_t a_paramIndex,
                       RE::SCRIPT_PARAM_TYPE a_type);
[[nodiscard]] std::string FormatNumberString(double a_value);
[[nodiscard]] bool ParseBooleanComparand(std::string_view a_text,
                                         bool a_defaultValue);
bool DrawNumericClauseValueEditor(const char *a_id, std::string &a_value,
                                  ValueEditorKind a_kind, float a_width);
bool DrawConditionParamEditor(const char *a_id, std::string &a_value,
                              RE::SCRIPT_PARAM_TYPE a_type, float a_width);
} // namespace sosr::ui::condition_editor
