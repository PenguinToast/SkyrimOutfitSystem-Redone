#include "Menu.h"

#include "RE/C/CommandTable.h"
#include "imgui_internal.h"
#include "ui/ConditionFunctionMetadata.h"
#include "ui/ConditionParamOptionCache.h"
#include "ui/components/EditableCombo.h"
#include "ui/components/PinnableTooltip.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cfloat>
#include <cstdio>
#include <functional>
#include <iomanip>
#include <sstream>
#include <span>
#include <string_view>

namespace sosr {
namespace {
using ConditionClause = ui::conditions::Clause;
using ConditionComparator = ui::conditions::Comparator;
using ConditionConnective = ui::conditions::Connective;
using ConditionDefinition = ui::conditions::Definition;

constexpr ImVec4 kDefaultConditionColor{0.55f, 0.55f, 0.55f, 1.0f};
constexpr char kIconTrash[] = "\xee\x86\x8c"; // ICON_LC_TRASH
constexpr char kIconGripVertical[] = "\xee\x83\xae"; // ICON_LC_GRIP_VERTICAL
constexpr char kConditionClausePayloadType[] = "SVS_CONDITION_CLAUSE";
constexpr std::string_view kDropdownSectionPrefix = "\x1fsection:";

enum class ConditionValueEditorKind : std::uint8_t {
  Unsupported,
  Integer,
  Number,
  CachedOption
};

struct ConditionFunctionInfo {
  std::string name;
  std::array<std::string, 2> parameterLabels{};
  std::array<bool, 2> parameterOptional{false, false};
  std::array<RE::SCRIPT_PARAM_TYPE, 2> parameterTypes{
      RE::SCRIPT_PARAM_TYPE::kForm, RE::SCRIPT_PARAM_TYPE::kForm};
  std::uint16_t parameterCount{0};
  bool returnsBooleanResult{false};
};

void DrawClauseDragHandle(const char *a_id, const ImVec2 a_size) {
  ImGui::InvisibleButton(a_id, a_size);

  const auto min = ImGui::GetItemRectMin();
  const auto max = ImGui::GetItemRectMax();
  const auto color =
      ImGui::GetColorU32(ImGui::IsItemHovered() ? ImGuiCol_Text
                                                : ImGuiCol_TextDisabled);
  auto *drawList = ImGui::GetWindowDrawList();
  const auto iconSize = ImGui::CalcTextSize(kIconGripVertical);
  drawList->AddText(
      ImVec2(min.x + ((max.x - min.x) - iconSize.x) * 0.5f,
             min.y + ((max.y - min.y) - iconSize.y) * 0.5f),
      color, kIconGripVertical);
}

void MoveConditionClause(std::vector<ConditionClause> &a_clauses,
                         const std::size_t a_sourceIndex,
                         const std::size_t a_targetIndex,
                         const bool a_insertAfter) {
  if (a_sourceIndex >= a_clauses.size() || a_targetIndex >= a_clauses.size()) {
    return;
  }

  auto destinationIndex = a_targetIndex;
  if (a_insertAfter) {
    ++destinationIndex;
  }
  if (a_sourceIndex < destinationIndex) {
    --destinationIndex;
  }
  if (a_sourceIndex == destinationIndex) {
    return;
  }

  auto clause = std::move(a_clauses[a_sourceIndex]);
  a_clauses.erase(a_clauses.begin() +
                  static_cast<std::ptrdiff_t>(a_sourceIndex));
  a_clauses.insert(a_clauses.begin() +
                       static_cast<std::ptrdiff_t>(destinationIndex),
                   std::move(clause));
}

ConditionValueEditorKind
GetEditorKindForParamType(RE::SCRIPT_PARAM_TYPE a_type);
RE::SCRIPT_PARAM_TYPE ResolveEditorParamType(std::string_view a_functionName,
                                             std::uint16_t a_paramIndex,
                                             RE::SCRIPT_PARAM_TYPE a_type);
const std::vector<ConditionFunctionInfo> &GetConditionFunctionInfos();
const ConditionFunctionInfo *FindConditionFunctionInfo(std::string_view a_name);

void CopyTextToBuffer(const std::string &a_text, char *a_buffer,
                      const std::size_t a_bufferSize) {
  if (a_bufferSize == 0) {
    return;
  }

  std::snprintf(a_buffer, a_bufferSize, "%s", a_text.c_str());
}

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

const char *ComparatorLabel(const ConditionComparator a_comparator) {
  switch (a_comparator) {
  case ConditionComparator::Equal:
    return "==";
  case ConditionComparator::NotEqual:
    return "!=";
  case ConditionComparator::Greater:
    return ">";
  case ConditionComparator::GreaterOrEqual:
    return ">=";
  case ConditionComparator::Less:
    return "<";
  case ConditionComparator::LessOrEqual:
    return "<=";
  }

  return "==";
}

bool IsBooleanComparator(const ConditionComparator a_comparator) {
  return a_comparator == ConditionComparator::Equal ||
         a_comparator == ConditionComparator::NotEqual;
}

const char *ConnectiveLabel(const ConditionConnective a_connective) {
  return a_connective == ConditionConnective::Or ? "OR" : "AND";
}

ConditionClause BuildDefaultPlayerClause() {
  ConditionClause clause;
  clause.functionName = "GetIsReference";
  clause.arguments[0] = "Player";
  clause.comparator = ConditionComparator::Equal;
  clause.comparand = "1";
  clause.connectiveToNext = ConditionConnective::And;
  return clause;
}

ConditionDefinition BuildDefaultPlayerCondition() {
  ConditionDefinition definition;
  definition.id = std::string(ui::conditions::kDefaultConditionId);
  definition.name = "Player";
  definition.description = "Applies to Player";
  definition.color = kDefaultConditionColor;
  definition.clauses.push_back(BuildDefaultPlayerClause());
  return definition;
}

ConditionDefinition BuildNewConditionTemplate() {
  ConditionDefinition definition;
  definition.color = kDefaultConditionColor;
  definition.clauses.push_back(BuildDefaultPlayerClause());
  return definition;
}

std::string BuildConditionId(const int a_nextConditionId) {
  return "condition-" + std::to_string((std::max)(a_nextConditionId, 1));
}

std::string BuildSectionEntry(std::string_view a_label) {
  std::string entry{kDropdownSectionPrefix};
  entry.append(a_label);
  return entry;
}

const ConditionDefinition *
FindConditionDefinitionById(const std::vector<ConditionDefinition> &a_conditions,
                            std::string_view a_id);

const ConditionDefinition *ResolveConditionDefinitionForValidation(
    const std::vector<ConditionDefinition> &a_conditions,
    const ConditionDefinition &a_draft, std::string_view a_conditionId) {
  if (!a_draft.id.empty() && a_conditionId == a_draft.id) {
    return std::addressof(a_draft);
  }
  return FindConditionDefinitionById(a_conditions, a_conditionId);
}

const ConditionDefinition *
FindConditionDefinitionById(const std::vector<ConditionDefinition> &a_conditions,
                            std::string_view a_id) {
  const auto it = std::ranges::find(a_conditions, a_id, &ConditionDefinition::id);
  return it != a_conditions.end() ? std::addressof(*it) : nullptr;
}

const ConditionDefinition *FindConditionDefinitionByName(
    const std::vector<ConditionDefinition> &a_conditions, std::string_view a_name,
    std::string_view a_excludedId = {}) {
  const auto it =
      std::ranges::find_if(a_conditions, [&](const ConditionDefinition &condition) {
        return condition.id != a_excludedId &&
               CompareTextInsensitive(condition.name, a_name) == 0;
      });
  return it != a_conditions.end() ? std::addressof(*it) : nullptr;
}

const ConditionFunctionInfo *ResolveConditionFunctionInfo(
    const ConditionClause &a_clause,
    const std::vector<ConditionDefinition> &a_conditions,
    std::optional<ConditionFunctionInfo> &a_customInfo) {
  if (!a_clause.customConditionId.empty()) {
    if (const auto *condition =
            FindConditionDefinitionById(a_conditions, a_clause.customConditionId);
        condition != nullptr) {
      a_customInfo = ConditionFunctionInfo{};
      a_customInfo->name = condition->name;
      a_customInfo->parameterCount = 0;
      a_customInfo->returnsBooleanResult = true;
      return std::addressof(*a_customInfo);
    }
  }

  return FindConditionFunctionInfo(a_clause.functionName);
}

std::string ResolveClauseDisplayName(
    const ConditionClause &a_clause,
    const std::vector<ConditionDefinition> &a_conditions) {
  if (!a_clause.customConditionId.empty()) {
    if (const auto *condition =
            FindConditionDefinitionById(a_conditions, a_clause.customConditionId);
        condition != nullptr) {
      return condition->name;
    }
  }
  return a_clause.functionName;
}

std::vector<std::string>
BuildConditionFunctionNames(const std::vector<ConditionDefinition> &a_conditions,
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

bool HasConditionDependencyCycle(const ConditionDefinition &a_draft,
                                 const std::vector<ConditionDefinition> &a_conditions) {
  std::vector<std::string_view> ids;
  ids.reserve(a_conditions.size() + (a_draft.id.empty() ? 0u : 1u));
  for (const auto &condition : a_conditions) {
    if (condition.id.empty()) {
      continue;
    }
    ids.push_back(condition.id);
  }
  if (!a_draft.id.empty() &&
      std::ranges::find(ids, std::string_view{a_draft.id}) == ids.end()) {
    ids.push_back(a_draft.id);
  }

  std::vector<std::uint8_t> states(ids.size(), 0);
  std::function<bool(std::string_view)> visit = [&](const std::string_view id) {
    const auto idIt = std::ranges::find(ids, id);
    if (idIt == ids.end()) {
      return false;
    }

    const auto index =
        static_cast<std::size_t>(std::distance(ids.begin(), idIt));
    if (states[index] == 1) {
      return true;
    }
    if (states[index] == 2) {
      return false;
    }

    states[index] = 1;
    if (const auto *condition =
            ResolveConditionDefinitionForValidation(a_conditions, a_draft, id);
        condition != nullptr) {
      for (const auto &clause : condition->clauses) {
        if (clause.customConditionId.empty()) {
          continue;
        }
        if (visit(clause.customConditionId)) {
          return true;
        }
      }
    }
    states[index] = 2;
    return false;
  };

  for (const auto id : ids) {
    if (visit(id)) {
      return true;
    }
  }
  return false;
}

std::string FormatNumberString(const double a_value) {
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

std::string ValidateConditionDraft(
    const ConditionDefinition &a_definition,
    const std::vector<ConditionDefinition> &a_conditions) {
  const auto name = TrimText(a_definition.name);
  if (name.empty()) {
    return "Condition name is required.";
  }

  if (FindConditionFunctionInfo(name) != nullptr) {
    return "Condition name conflicts with an existing condition function.";
  }

  if (FindConditionDefinitionByName(a_conditions, name, a_definition.id) !=
      nullptr) {
    return "Condition name conflicts with another custom condition.";
  }

  if (a_definition.clauses.empty()) {
    return "At least one clause is required.";
  }

  if (HasConditionDependencyCycle(a_definition, a_conditions)) {
    return "Custom condition references must not contain circular dependencies.";
  }

  for (std::size_t index = 0; index < a_definition.clauses.size(); ++index) {
    const auto &clause = a_definition.clauses[index];
    const auto functionName = TrimText(clause.functionName);
    std::optional<ConditionFunctionInfo> customFunctionInfo;
    const auto *functionInfo =
        ResolveConditionFunctionInfo(clause, a_conditions, customFunctionInfo);
    if (!functionInfo) {
      return "Unknown or unsupported condition function in clause " +
             std::to_string(index + 1) + ".";
    }
    if (!clause.customConditionId.empty() && clause.customConditionId == a_definition.id &&
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

      const auto editorKind =
          GetEditorKindForParamType(ResolveEditorParamType(
              functionInfo->name, paramIndex,
              functionInfo->parameterTypes[paramIndex]));
      if (editorKind == ConditionValueEditorKind::Unsupported) {
        return functionInfo->parameterLabels[paramIndex] +
               " uses an unsupported parameter type in clause " +
               std::to_string(index + 1) + ".";
      }

      if ((editorKind == ConditionValueEditorKind::Integer ||
           editorKind == ConditionValueEditorKind::Number) &&
          !argument.empty()) {
        try {
          if (editorKind == ConditionValueEditorKind::Integer) {
            (void)std::stoi(argument);
          } else {
            (void)std::stod(argument);
          }
        } catch (const std::exception &) {
          return functionInfo->parameterLabels[paramIndex] +
                 (editorKind == ConditionValueEditorKind::Integer
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

ConditionValueEditorKind
GetEditorKindForParamType(const RE::SCRIPT_PARAM_TYPE a_type) {
  switch (a_type) {
  case RE::SCRIPT_PARAM_TYPE::kChar:
  case RE::SCRIPT_PARAM_TYPE::kInt:
  case RE::SCRIPT_PARAM_TYPE::kStage:
  case RE::SCRIPT_PARAM_TYPE::kRelationshipRank:
    return ConditionValueEditorKind::Integer;
  case RE::SCRIPT_PARAM_TYPE::kFloat:
    return ConditionValueEditorKind::Number;
  default:
    return ui::conditions::ConditionParamOptionCache::Supports(a_type)
               ? ConditionValueEditorKind::CachedOption
               : ConditionValueEditorKind::Unsupported;
  }
}

bool SupportsTypedParamInput(const RE::SCRIPT_PARAM_TYPE a_type) {
  return GetEditorKindForParamType(a_type) !=
         ConditionValueEditorKind::Unsupported;
}

RE::SCRIPT_PARAM_TYPE ResolveEditorParamType(std::string_view a_functionName,
                                             const std::uint16_t a_paramIndex,
                                             const RE::SCRIPT_PARAM_TYPE a_type) {
  if (a_paramIndex == 0 &&
      CompareTextInsensitive(a_functionName, "GetIsID") == 0) {
    return RE::SCRIPT_PARAM_TYPE::kActorBase;
  }

  return a_type;
}

bool SupportsTypedFunction(const RE::SCRIPT_FUNCTION &a_command) {
  if (!a_command.conditionFunction || a_command.numParams > 2 ||
      ui::conditions::IsObsoleteConditionFunction(a_command)) {
    return false;
  }

  for (std::uint16_t paramIndex = 0; paramIndex < a_command.numParams;
       ++paramIndex) {
    const auto paramType =
        ResolveEditorParamType(
            a_command.functionName ? a_command.functionName : "", paramIndex,
            a_command.params ? a_command.params[paramIndex].paramType.get()
                             : RE::SCRIPT_PARAM_TYPE::kForm);
    if (!SupportsTypedParamInput(paramType)) {
      return false;
    }
  }

  return true;
}

bool DrawNumericClauseValueEditor(const char *a_id, std::string &a_value,
                                  const ConditionValueEditorKind a_kind,
                                  const float a_width) {
  if (a_kind == ConditionValueEditorKind::Unsupported ||
      a_kind == ConditionValueEditorKind::CachedOption) {
    ImGui::BeginDisabled();
    ImGui::SetNextItemWidth(a_width);
    char buffer[32] = "Unsupported";
    ImGui::InputText(a_id, buffer, sizeof(buffer), ImGuiInputTextFlags_ReadOnly);
    ImGui::EndDisabled();
    return false;
  }

  ImGui::SetNextItemWidth(a_width);

  if (a_kind == ConditionValueEditorKind::Integer) {
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

  if (a_kind == ConditionValueEditorKind::Number) {
    double numericValue = 0.0;
    if (!a_value.empty()) {
      try {
        numericValue = std::stod(a_value);
      } catch (const std::exception &) {
      }
    }

    if (ImGui::InputDouble(a_id, &numericValue, 0.0, 0.0, "%.3f")) {
      a_value = FormatNumberString(numericValue);
      return true;
    }
    return false;
  }

  return false;
}

bool DrawConditionParamEditor(const char *a_id, std::string &a_value,
                              const RE::SCRIPT_PARAM_TYPE a_type,
                              const float a_width) {
  const auto editorKind = GetEditorKindForParamType(a_type);
  if (editorKind == ConditionValueEditorKind::Integer ||
      editorKind == ConditionValueEditorKind::Number) {
    return DrawNumericClauseValueEditor(a_id, a_value, editorKind, a_width);
  }

  if (editorKind == ConditionValueEditorKind::CachedOption) {
    auto &optionCache = ui::conditions::ConditionParamOptionCache::Get();
    const auto state = optionCache.Ensure(a_type);
    if (state == ui::conditions::ConditionParamOptionCache::State::Ready) {
      const auto *options = optionCache.GetOptions(a_type);
      if (options) {
        return ui::components::DrawSearchableDropdown(
            a_id, "Select value", a_value, *options, a_width);
      }
    } else if (state ==
               ui::conditions::ConditionParamOptionCache::State::Loading) {
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

void DrawHoverDescription(const std::string_view a_id,
                          const std::string_view a_text,
                          const float a_delaySeconds = 0.45f,
                          const ImGuiHoveredFlags a_hoveredFlags = 0) {
  if (!ImGui::IsItemHovered(a_hoveredFlags) ||
      ImGui::GetCurrentContext()->HoveredIdTimer < a_delaySeconds) {
    return;
  }

  ui::components::DrawPinnableTooltip(a_id, true, [&]() {
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 360.0f);
    ImGui::TextUnformatted(a_text.data(), a_text.data() + a_text.size());
    ImGui::PopTextWrapPos();
  });
}

void DrawClauseHeaderCell(const char *a_id, const char *a_label,
                          const std::string_view a_tooltip) {
  ImGui::TextDisabled("%s", a_label);
  DrawHoverDescription(a_id, a_tooltip);
}

void DrawConditionColorSwatch(const char *a_id, const ImVec4 &a_color,
                              const std::string_view a_tooltip) {
  ImGui::ColorButton(a_id, a_color,
                     ImGuiColorEditFlags_NoTooltip |
                         ImGuiColorEditFlags_NoDragDrop |
                         ImGuiColorEditFlags_NoBorder,
                     ImVec2(ImGui::GetFrameHeight() - 2.0f,
                            ImGui::GetFrameHeight() - 2.0f));
  DrawHoverDescription(std::string(a_id), a_tooltip);
}


const std::vector<ConditionFunctionInfo> &GetConditionFunctionInfos() {
  static const auto infos = [] {
    std::vector<ConditionFunctionInfo> result;

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

      if (std::ranges::find_if(result, [&](const ConditionFunctionInfo &info) {
            return CompareTextInsensitive(info.name, name) == 0;
          }) != result.end()) {
        continue;
      }

      ConditionFunctionInfo info;
      info.name = name;
      info.parameterCount = command.numParams;
      info.returnsBooleanResult =
          ui::conditions::ReturnsBooleanConditionResult(info.name);
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

const ConditionFunctionInfo *
FindConditionFunctionInfo(std::string_view a_name) {
  const auto &infos = GetConditionFunctionInfos();
  const auto it = std::ranges::find_if(infos, [&](const auto &info) {
    return CompareTextInsensitive(info.name, a_name) == 0;
  });
  return it != infos.end() ? std::addressof(*it) : nullptr;
}

float MeasureConditionRowHeight(const ConditionDefinition &a_definition,
                                const float a_wrapWidth) {
  const auto &style = ImGui::GetStyle();
  const auto nameHeight = ImGui::GetTextLineHeight();
  if (a_definition.description.empty()) {
    return style.FramePadding.y * 2.0f + nameHeight;
  }

  const auto descriptionSize = ImGui::CalcTextSize(
      a_definition.description.c_str(), nullptr, false, a_wrapWidth);
  return style.FramePadding.y * 2.0f + nameHeight + style.ItemSpacing.y +
         descriptionSize.y;
}
} // namespace

void Menu::EnsureDefaultConditions() {
  if (!conditions_.empty()) {
    return;
  }

  conditions_.push_back(BuildDefaultPlayerCondition());
  nextConditionId_ = 2;
}

int Menu::AllocateConditionEditorWindowSlot() const {
  int slot = 1;
  while (true) {
    const auto it = std::ranges::find(conditionEditors_, slot,
                                      &ConditionEditorState::windowSlot);
    if (it == conditionEditors_.end()) {
      return slot;
    }
    ++slot;
  }
}

void Menu::OpenNewConditionDialog() {
  ConditionEditorState editor;
  editor.windowSlot = AllocateConditionEditorWindowSlot();
  editor.draft = BuildNewConditionTemplate();
  editor.isNew = true;
  editor.focusOnNextDraw = true;
  conditionEditors_.push_back(std::move(editor));
}

void Menu::OpenConditionEditorDialog(const std::size_t a_index) {
  if (a_index >= conditions_.size()) {
    return;
  }

  const auto &condition = conditions_[a_index];
  const auto existingIt =
      std::ranges::find(conditionEditors_, condition.id,
                        &ConditionEditorState::sourceConditionId);
  if (existingIt != conditionEditors_.end()) {
    existingIt->focusOnNextDraw = true;
    existingIt->open = true;
    return;
  }

  ConditionEditorState editor;
  editor.windowSlot = AllocateConditionEditorWindowSlot();
  editor.sourceConditionId = condition.id;
  editor.draft = condition;
  editor.focusOnNextDraw = true;
  conditionEditors_.push_back(std::move(editor));
}

void Menu::OpenConditionEditorDialogById(const std::string_view a_conditionId) {
  const auto it =
      std::ranges::find(conditions_, a_conditionId, &ConditionDefinition::id);
  if (it == conditions_.end()) {
    return;
  }
  OpenConditionEditorDialog(
      static_cast<std::size_t>(std::distance(conditions_.begin(), it)));
}

bool Menu::SaveConditionEditor(ConditionEditorState &a_editor) {
  if (const auto validationError =
          ValidateConditionDraft(a_editor.draft, conditions_);
      !validationError.empty()) {
    a_editor.error = validationError;
    return false;
  }

  for (std::size_t index = 0; index < a_editor.draft.clauses.size(); ++index) {
    auto &clause = a_editor.draft.clauses[index];
    clause.functionName = TrimText(clause.functionName);
    clause.arguments[0] = TrimText(clause.arguments[0]);
    clause.arguments[1] = TrimText(clause.arguments[1]);
    clause.comparand = TrimText(clause.comparand);

    std::optional<ConditionFunctionInfo> customFunctionInfo;
    const auto *functionInfo =
        ResolveConditionFunctionInfo(clause, conditions_, customFunctionInfo);
    if (!functionInfo) {
      a_editor.error = "Unknown or unsupported condition function in clause " +
                       std::to_string(index + 1) + ".";
      return false;
    }
    if (!clause.customConditionId.empty()) {
      if (clause.customConditionId == a_editor.draft.id &&
          !a_editor.draft.id.empty()) {
        a_editor.error = "A condition cannot reference itself in clause " +
                         std::to_string(index + 1) + ".";
        return false;
      }
      clause.arguments[0].clear();
      clause.arguments[1].clear();
      clause.functionName.clear();
    }

    for (std::uint16_t paramIndex = 0;
         paramIndex < functionInfo->parameterCount && paramIndex < 2;
         ++paramIndex) {
      if (!functionInfo->parameterOptional[paramIndex] &&
          clause.arguments[paramIndex].empty()) {
        a_editor.error = functionInfo->parameterLabels[paramIndex] +
                         " is required in clause " + std::to_string(index + 1) +
                         ".";
        return false;
      }

      const auto editorKind =
          GetEditorKindForParamType(ResolveEditorParamType(
              clause.functionName, paramIndex,
              functionInfo->parameterTypes[paramIndex]));
      if (editorKind == ConditionValueEditorKind::Unsupported) {
        a_editor.error = functionInfo->parameterLabels[paramIndex] +
                         " uses an unsupported parameter type in clause " +
                         std::to_string(index + 1) + ".";
        return false;
      }
      if (editorKind == ConditionValueEditorKind::Integer &&
          !clause.arguments[paramIndex].empty()) {
        try {
          clause.arguments[paramIndex] =
              std::to_string(std::stoi(clause.arguments[paramIndex]));
        } catch (const std::exception &) {
          a_editor.error = functionInfo->parameterLabels[paramIndex] +
                           " must be an integer in clause " +
                           std::to_string(index + 1) + ".";
          return false;
        }
      } else if (editorKind == ConditionValueEditorKind::Number &&
                 !clause.arguments[paramIndex].empty()) {
        try {
          clause.arguments[paramIndex] =
              FormatNumberString(std::stod(clause.arguments[paramIndex]));
        } catch (const std::exception &) {
          a_editor.error = functionInfo->parameterLabels[paramIndex] +
                           " must be numeric in clause " +
                           std::to_string(index + 1) + ".";
          return false;
        }
      }
    }

    if (functionInfo->returnsBooleanResult) {
      if (!IsBooleanComparator(clause.comparator)) {
        a_editor.error =
            "Boolean-return conditions only support == and != in clause " +
            std::to_string(index + 1) + ".";
        return false;
      }
      clause.comparand = ParseBooleanComparand(clause.comparand, false) ? "1"
                                                                        : "0";
    } else {
      if (clause.comparand.empty()) {
        a_editor.error = "A comparison value is required in clause " +
                         std::to_string(index + 1) + ".";
        return false;
      }

      try {
        clause.comparand = FormatNumberString(std::stod(clause.comparand));
      } catch (const std::exception &) {
        a_editor.error = "Comparison value must be numeric in clause " +
                         std::to_string(index + 1) + ".";
        return false;
      }
    }
  }

  a_editor.draft.name = TrimText(a_editor.draft.name);
  a_editor.draft.color.w = 1.0f;
  if (a_editor.draft.id.empty()) {
    a_editor.draft.id = BuildConditionId(nextConditionId_++);
  }
  if (a_editor.isNew) {
    conditions_.push_back(a_editor.draft);
    a_editor.sourceConditionId = a_editor.draft.id;
    a_editor.isNew = false;
  } else {
    const auto it = std::ranges::find(conditions_, a_editor.sourceConditionId,
                                      &ui::conditions::Definition::id);
    if (it == conditions_.end()) {
      a_editor.error = "The condition being edited no longer exists.";
      return false;
    }
    *it = a_editor.draft;
  }

  a_editor.error.clear();
  return true;
}

bool Menu::DrawConditionTab() {
  EnsureDefaultConditions();

  if (ImGui::Button("Add New")) {
    OpenNewConditionDialog();
  }
  ImGui::SameLine();
  ImGui::TextDisabled("%zu condition%s", conditions_.size(),
                      conditions_.size() == 1 ? "" : "s");
  ImGui::Spacing();

  if (!ImGui::BeginTable("##conditions-table", 1,
                         ImGuiTableFlags_SizingStretchProp |
                             ImGuiTableFlags_RowBg |
                             ImGuiTableFlags_BordersInnerV,
                         ImVec2(0.0f, 0.0f))) {
    return false;
  }

  ImGui::TableSetupColumn("Condition", ImGuiTableColumnFlags_WidthStretch);
  bool rowClicked = false;

  for (std::size_t index = 0; index < conditions_.size(); ++index) {
    const auto &condition = conditions_[index];
    const auto rowWrapWidth =
        (std::max)(ImGui::GetContentRegionAvail().x - 18.0f, 120.0f);
    const auto rowHeight =
        MeasureConditionRowHeight(condition, rowWrapWidth);

    ImGui::TableNextRow(0, rowHeight);
    ImGui::TableSetColumnIndex(0);
    ImGui::PushID(static_cast<int>(index));

    const auto width = ImGui::GetContentRegionAvail().x;
    ImGui::InvisibleButton("##condition-row", ImVec2(width, rowHeight));
    rowClicked |= ImGui::IsItemClicked(ImGuiMouseButton_Left);
    const auto hovered = ImGui::IsItemHovered();
    if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
      OpenConditionEditorDialog(index);
    }

    const auto min = ImGui::GetItemRectMin();
    const auto max = ImGui::GetItemRectMax();
    const auto stripeWidth = 6.0f;
    const auto rounding = ImGui::GetStyle().FrameRounding;
    auto *drawList = ImGui::GetWindowDrawList();
    const auto bodyColor =
        hovered ? IM_COL32(42, 42, 44, 240) : IM_COL32(34, 34, 36, 225);
    drawList->AddRectFilled(min, max, bodyColor, rounding);
    drawList->AddRect(
        min, max,
        ImGui::GetColorU32(ImVec4(condition.color.x, condition.color.y,
                                  condition.color.z, 0.75f)),
        rounding);
    drawList->AddRectFilled(min, ImVec2(min.x + stripeWidth, max.y),
                            ImGui::GetColorU32(condition.color), rounding,
                            ImDrawFlags_RoundCornersTopLeft |
                                ImDrawFlags_RoundCornersBottomLeft);

    const auto contentMin = ImVec2(min.x + stripeWidth + 10.0f,
                                   min.y + ImGui::GetStyle().FramePadding.y);
    const auto textColor = ImGui::GetColorU32(
        ImVec4(condition.color.x, condition.color.y, condition.color.z, 1.0f));
    const auto clipRect = ImVec4(contentMin.x, min.y, max.x - 8.0f, max.y);
    drawList->PushClipRect(ImVec2(clipRect.x, clipRect.y),
                           ImVec2(clipRect.z, clipRect.w), true);
    drawList->AddText(contentMin, textColor, condition.name.c_str());
    if (!condition.description.empty()) {
      drawList->AddText(
          ImGui::GetFont(), ImGui::GetFontSize(),
          ImVec2(contentMin.x, contentMin.y + ImGui::GetTextLineHeight() +
                                   ImGui::GetStyle().ItemSpacing.y),
          ImGui::GetColorU32(ImVec4(0.70f, 0.72f, 0.75f, 1.0f)),
          condition.description.c_str(), nullptr, rowWrapWidth);
    }
    drawList->PopClipRect();

    if (ImGui::BeginDragDropSource()) {
      DraggedConditionPayload payload{};
      std::snprintf(payload.conditionId.data(), payload.conditionId.size(), "%s",
                    condition.id.c_str());
      ImGui::SetDragDropPayload("SVS_CONDITION", &payload, sizeof(payload));
      ImGui::TextUnformatted(condition.name.c_str());
      if (!condition.description.empty()) {
        ImGui::TextUnformatted(condition.description.c_str());
      }
      ImGui::EndDragDropSource();
    }

    ImGui::PopID();
  }

  ImGui::EndTable();
  return rowClicked;
}

void Menu::DrawConditionEditorDialog() {
  ui::conditions::ConditionParamOptionCache::Get().Continue(16.0);

  for (auto &editor : conditionEditors_) {
    if (!editor.open) {
      continue;
    }

    std::string title = editor.isNew ? "New Condition" : editor.draft.name;
    if (title.empty()) {
      title = "Untitled";
    }
    title.insert(0, "Condition Editor [");
    title.push_back(']');
    title.append("###");
    title.append("condition-editor-");
    title.append(std::to_string((std::max)(editor.windowSlot, 1)));

    ImGui::SetNextWindowSize(ImVec2(1180.0f, 560.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(900.0f, 480.0f),
                                        ImVec2(FLT_MAX, FLT_MAX));
    if (editor.focusOnNextDraw) {
      ImGui::SetNextWindowFocus();
      editor.focusOnNextDraw = false;
    }

    if (!ImGui::Begin(title.c_str(), &editor.open,
                      ImGuiWindowFlags_NoCollapse)) {
      ImGui::End();
      continue;
    }

    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
      focusedConditionEditorWindowSlot_ = editor.windowSlot;
    }

    const auto footerHeight =
        ImGui::GetFrameHeightWithSpacing() * 2.0f +
        ImGui::GetStyle().ItemSpacing.y * 3.0f +
        ((!editor.error.empty()) ? (ImGui::GetTextLineHeightWithSpacing() * 2.0f)
                                 : 0.0f);

    if (ImGui::BeginChild("##condition-editor-body",
                          ImVec2(0.0f, -footerHeight), ImGuiChildFlags_None,
                          ImGuiWindowFlags_NoScrollbar |
                              ImGuiWindowFlags_NoScrollWithMouse)) {
      char nameBuffer[256];
      CopyTextToBuffer(editor.draft.name, nameBuffer, sizeof(nameBuffer));
      ImGui::SetNextItemWidth(
          (std::min)(420.0f, ImGui::GetContentRegionAvail().x));
      if (ImGui::InputTextWithHint("##name", "Name", nameBuffer,
                                   sizeof(nameBuffer))) {
        editor.draft.name = nameBuffer;
      }
      DrawHoverDescription("conditions:editor:name",
                           "Friendly name shown in the Conditions catalog.");

      char descriptionBuffer[512];
      CopyTextToBuffer(editor.draft.description, descriptionBuffer,
                       sizeof(descriptionBuffer));
      ImGui::SetNextItemWidth(
          (std::min)(520.0f, ImGui::GetContentRegionAvail().x));
      if (ImGui::InputTextWithHint("##description", "Description",
                                   descriptionBuffer,
                                   sizeof(descriptionBuffer))) {
        editor.draft.description = descriptionBuffer;
      }
      DrawHoverDescription(
          "conditions:editor:description",
          "Optional description shown on the condition widget.");

      ImVec4 color = editor.draft.color;
      ImGui::SetNextItemWidth(
          (std::min)(260.0f, ImGui::GetContentRegionAvail().x));
      if (ImGui::ColorEdit3("##color", &color.x,
                            ImGuiColorEditFlags_DisplayRGB)) {
        editor.draft.color = ImVec4(color.x, color.y, color.z, 1.0f);
      }
      DrawHoverDescription(
          "conditions:editor:color",
          "Associated display color used by the Conditions catalog row.");

      ImGui::Spacing();
      ImGui::TextUnformatted("Clauses");
      ImGui::Separator();

      const auto conditionFunctionNames =
          BuildConditionFunctionNames(conditions_, editor.sourceConditionId);
      const auto clausePaneHeight =
          (std::max)(220.0f, ImGui::GetContentRegionAvail().y);
      const auto &style = ImGui::GetStyle();
      const auto editButtonWidth =
          ImGui::CalcTextSize("Edit").x + style.FramePadding.x * 2.0f;
      const auto deleteButtonWidth =
          ImGui::CalcTextSize(kIconTrash).x + style.FramePadding.x * 2.0f;
      const auto actionsColumnWidth =
          editButtonWidth + style.ItemSpacing.x + deleteButtonWidth +
          style.CellPadding.x * 2.0f;
      if (ImGui::BeginChild("##condition-clauses",
                            ImVec2(0.0f, clausePaneHeight),
                            ImGuiChildFlags_Borders)) {
        constexpr ImGuiTableFlags clauseTableFlags =
            ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV |
            ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_ScrollX |
            ImGuiTableFlags_Resizable;
        if (ImGui::BeginTable("##condition-clause-table", 8, clauseTableFlags,
                              ImVec2(0.0f, 0.0f))) {
          const auto tableOuterRect = ImGui::GetCurrentTable()->OuterRect;
          ImGui::TableSetupColumn("",
                                  ImGuiTableColumnFlags_WidthFixed |
                                      ImGuiTableColumnFlags_NoResize,
                                  18.0f);
          ImGui::TableSetupColumn("Function", ImGuiTableColumnFlags_WidthFixed,
                                  240.0f);
          ImGui::TableSetupColumn("Arg 1", ImGuiTableColumnFlags_WidthFixed,
                                  170.0f);
          ImGui::TableSetupColumn("Arg 2", ImGuiTableColumnFlags_WidthFixed,
                                  170.0f);
          ImGui::TableSetupColumn("Comparator",
                                  ImGuiTableColumnFlags_WidthFixed, 120.0f);
          ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed,
                                  130.0f);
          ImGui::TableSetupColumn("Join", ImGuiTableColumnFlags_WidthFixed,
                                  90.0f);
          ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed,
                                  actionsColumnWidth);

          ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
          ImGui::TableSetColumnIndex(0);
          ImGui::TextDisabled(" ");
          DrawHoverDescription("conditions:help:move", "Drag to reorder clauses.");
          ImGui::TableSetColumnIndex(1);
          DrawClauseHeaderCell(
              "conditions:help:function", "Function",
              "Condition function to evaluate for this clause.");
          ImGui::TableSetColumnIndex(2);
          DrawClauseHeaderCell("conditions:help:arg1", "Arg 1",
                               "First function argument. Input type follows "
                               "the selected function.");
          ImGui::TableSetColumnIndex(3);
          DrawClauseHeaderCell(
              "conditions:help:arg2", "Arg 2",
              "Second function argument when the selected function needs one.");
          ImGui::TableSetColumnIndex(4);
          DrawClauseHeaderCell("conditions:help:comparator", "Comparator",
                               "How the function result is compared.");
          ImGui::TableSetColumnIndex(5);
          DrawClauseHeaderCell(
              "conditions:help:value", "Value",
              "Value compared against the function result. Boolean-returning "
              "functions use a true/false checkbox.");
          ImGui::TableSetColumnIndex(6);
          DrawClauseHeaderCell("conditions:help:join", "Join",
                               "How this clause combines with the next one.");
          ImGui::TableSetColumnIndex(7);
          DrawClauseHeaderCell("conditions:help:actions", "Actions",
                               "Remove this clause.");

          const auto *activePayload = ImGui::GetDragDropPayload();
          const bool reorderPreviewActive =
              activePayload && activePayload->Data != nullptr &&
              activePayload->IsDataType(kConditionClausePayloadType) &&
              activePayload->DataSize == sizeof(int);
          float insertionLineY = -1.0f;
          float insertionLineX1 = -1.0f;
          float insertionLineX2 = -1.0f;
          int hoveredClauseReorderIndex = -1;
          bool hoveredClauseInsertAfter = false;
          std::optional<std::size_t> acceptedSourceClauseIndex;
          bool acceptedInsertAfter = false;

          for (std::size_t index = 0; index < editor.draft.clauses.size();) {
            ImGui::PushID(static_cast<int>(index));
            auto &clause = editor.draft.clauses[index];
            std::optional<ConditionFunctionInfo> customFunctionInfo;
            const auto *functionInfo =
                ResolveConditionFunctionInfo(clause, conditions_, customFunctionInfo);
            auto selectedFunctionName =
                functionInfo ? functionInfo->name : clause.functionName;

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            DrawClauseDragHandle("##drag-handle",
                                 ImVec2(ImGui::GetContentRegionAvail().x,
                                        ImGui::GetFrameHeight()));
            DrawHoverDescription(
                "conditions:editor:drag:" + std::to_string(index),
                "Drag this handle to reorder the clause.");
            if (ImGui::BeginDragDropSource()) {
              const int payloadIndex = static_cast<int>(index);
              ImGui::SetDragDropPayload(kConditionClausePayloadType,
                                        &payloadIndex, sizeof(payloadIndex));
              ImGui::Text("Move clause %zu", index + 1);
              ImGui::EndDragDropSource();
            }
            ImGui::TableSetColumnIndex(1);
            const auto *customCondition =
                clause.customConditionId.empty()
                    ? nullptr
                    : FindConditionDefinitionById(conditions_,
                                                  clause.customConditionId);
            float functionWidth = ImGui::GetContentRegionAvail().x;
            if (customCondition != nullptr) {
              const float swatchSize = ImGui::GetFrameHeight() - 2.0f;
              const float spacing = ImGui::GetStyle().ItemSpacing.x;
              DrawConditionColorSwatch(
                  "##custom-condition-color", customCondition->color,
                  "Referenced custom condition color: " +
                      customCondition->name);
              ImGui::SameLine(0.0f, spacing);
              functionWidth = (std::max)(0.0f, functionWidth - swatchSize - spacing);
            }
            if (ui::components::DrawSearchableDropdown(
                    "##function", "Condition function", selectedFunctionName,
                    conditionFunctionNames, functionWidth)) {
              clause.arguments[0].clear();
              clause.arguments[1].clear();
              if (const auto *selectedCustomCondition = FindConditionDefinitionByName(
                      conditions_, selectedFunctionName, editor.sourceConditionId);
                  selectedCustomCondition != nullptr) {
                clause.customConditionId = selectedCustomCondition->id;
                clause.functionName.clear();
              } else {
                clause.customConditionId.clear();
                clause.functionName = selectedFunctionName;
              }
              customFunctionInfo.reset();
              functionInfo = ResolveConditionFunctionInfo(
                  clause, conditions_, customFunctionInfo);
              if (functionInfo && functionInfo->returnsBooleanResult &&
                  !IsBooleanComparator(clause.comparator)) {
                clause.comparator = ConditionComparator::Equal;
              }
            }
            DrawHoverDescription(
                "conditions:editor:function:" + std::to_string(index),
                "Select a condition function. Only functions whose parameters "
                "SVS can model with typed inputs are shown.");

            const auto argumentCount =
                functionInfo ? functionInfo->parameterCount : std::uint16_t{2};
            for (std::uint16_t paramIndex = 0; paramIndex < 2; ++paramIndex) {
              ImGui::TableSetColumnIndex(2 + paramIndex);
              ImGui::BeginDisabled(paramIndex >= argumentCount);
              if (functionInfo && paramIndex < argumentCount) {
                DrawConditionParamEditor(
                    paramIndex == 0 ? "##arg1" : "##arg2",
                    clause.arguments[paramIndex],
                    ResolveEditorParamType(
                        clause.functionName, paramIndex,
                        functionInfo->parameterTypes[paramIndex]),
                    ImGui::GetContentRegionAvail().x);
              } else {
                ImGui::BeginDisabled();
                char buffer[16] = "";
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::InputText(paramIndex == 0 ? "##arg1" : "##arg2",
                                 buffer, sizeof(buffer),
                                 ImGuiInputTextFlags_ReadOnly);
                ImGui::EndDisabled();
              }
              ImGui::EndDisabled();

              std::string tooltip =
                  functionInfo && paramIndex < argumentCount
                      ? functionInfo->parameterLabels[paramIndex]
                      : "Unused argument for the current function.";
              if (functionInfo && paramIndex < argumentCount &&
                  functionInfo->parameterOptional[paramIndex]) {
                tooltip.append(" Optional.");
              } else if (functionInfo && paramIndex < argumentCount) {
                tooltip.append(" Required.");
              }
              DrawHoverDescription(
                  "conditions:editor:arg:" + std::to_string(index) + ":" +
                      std::to_string(paramIndex),
                  tooltip);
            }
            for (std::uint16_t paramIndex = argumentCount; paramIndex < 2;
                 ++paramIndex) {
              clause.arguments[paramIndex].clear();
            }

            ImGui::TableSetColumnIndex(4);
            constexpr std::array kComparatorLabels = {"==", "!=", ">",
                                                      ">=", "<",  "<="};
            constexpr std::array kBooleanComparators = {
                ConditionComparator::Equal, ConditionComparator::NotEqual};
            constexpr std::array kAllComparators = {
                ConditionComparator::Equal,
                ConditionComparator::NotEqual,
                ConditionComparator::Greater,
                ConditionComparator::GreaterOrEqual,
                ConditionComparator::Less,
                ConditionComparator::LessOrEqual};
            const std::span<const ConditionComparator> availableComparators =
                functionInfo && functionInfo->returnsBooleanResult
                    ? std::span<const ConditionComparator>(
                          kBooleanComparators.begin(), kBooleanComparators.end())
                    : std::span<const ConditionComparator>(
                          kAllComparators.begin(), kAllComparators.end());
            if (functionInfo && functionInfo->returnsBooleanResult &&
                !IsBooleanComparator(clause.comparator)) {
              clause.comparator = ConditionComparator::Equal;
            }
            const auto comparatorIt =
                std::ranges::find(availableComparators, clause.comparator);
            const int comparatorIndex =
                comparatorIt != availableComparators.end()
                    ? static_cast<int>(std::distance(availableComparators.begin(),
                                                    comparatorIt))
                    : 0;
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            if (ImGui::BeginCombo("##comparator",
                                  ComparatorLabel(
                                      availableComparators[comparatorIndex]))) {
              for (int optionIndex = 0;
                   optionIndex <
                   static_cast<int>(availableComparators.size());
                   ++optionIndex) {
                const bool selected = comparatorIndex == optionIndex;
                if (ImGui::Selectable(
                        ComparatorLabel(availableComparators[optionIndex]),
                        selected)) {
                  clause.comparator = availableComparators[optionIndex];
                }
                if (selected) {
                  ImGui::SetItemDefaultFocus();
                }
              }
              ImGui::EndCombo();
            }
            DrawHoverDescription("conditions:editor:comparator:" +
                                     std::to_string(index),
                                 "Comparison operator applied to the function "
                                 "result.");

            ImGui::TableSetColumnIndex(5);
            if (functionInfo && functionInfo->returnsBooleanResult) {
              bool boolComparand = ParseBooleanComparand(clause.comparand, true);
              if (ImGui::Checkbox("##comparand", &boolComparand)) {
                clause.comparand = boolComparand ? "1" : "0";
              } else if (clause.comparand.empty() || clause.comparand != "0" &&
                                                     clause.comparand != "1") {
                clause.comparand = boolComparand ? "1" : "0";
              }
            } else {
              DrawNumericClauseValueEditor("##comparand", clause.comparand,
                                           ConditionValueEditorKind::Number,
                                           ImGui::GetContentRegionAvail().x);
            }
            DrawHoverDescription("conditions:editor:value:" +
                                     std::to_string(index),
                                 functionInfo && functionInfo->returnsBooleanResult
                                     ? "Checked stores 1 (true). Unchecked "
                                       "stores 0 (false)."
                                     : "Numeric value compared against the "
                                       "function result.");

            ImGui::TableSetColumnIndex(6);
            const bool hasNextClause = index + 1 < editor.draft.clauses.size();
            ImGui::BeginDisabled(!hasNextClause);
            int connectiveIndex =
                clause.connectiveToNext == ConditionConnective::Or ? 1 : 0;
            constexpr std::array connectiveLabels = {"AND", "OR"};
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            if (ImGui::BeginCombo("##join",
                                  connectiveLabels[connectiveIndex])) {
              for (int optionIndex = 0; optionIndex < 2; ++optionIndex) {
                const bool selected = connectiveIndex == optionIndex;
                if (ImGui::Selectable(connectiveLabels[optionIndex],
                                      selected)) {
                  connectiveIndex = optionIndex;
                  clause.connectiveToNext = connectiveIndex == 1
                                                ? ConditionConnective::Or
                                                : ConditionConnective::And;
                }
                if (selected) {
                  ImGui::SetItemDefaultFocus();
                }
              }
              ImGui::EndCombo();
            }
            ImGui::EndDisabled();
            DrawHoverDescription(
                "conditions:editor:join:" + std::to_string(index),
                "How this clause combines with the next clause.");

            ImGui::TableSetColumnIndex(7);
            const bool hasCustomEditTarget = !clause.customConditionId.empty();
            if (hasCustomEditTarget) {
              if (ImGui::Button("Edit", ImVec2(editButtonWidth, 0.0f))) {
                OpenConditionEditorDialogById(clause.customConditionId);
              }
              DrawHoverDescription("conditions:editor:edit-custom:" +
                                       std::to_string(index),
                                   "Open the referenced custom condition.");
              if (editor.draft.clauses.size() > 1) {
                ImGui::SameLine();
              }
            }
            if (editor.draft.clauses.size() > 1) {
              auto *theme = ThemeConfig::GetSingleton();
              ImGui::PushStyleColor(ImGuiCol_Button,
                                    theme->GetColor("DECLINE", 0.78f));
              ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                                    theme->GetColor("DECLINE", 0.95f));
              ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                                    theme->GetColor("DECLINE", 0.90f));
              if (ImGui::Button(kIconTrash,
                                ImVec2(deleteButtonWidth, 0.0f))) {
                editor.draft.clauses.erase(editor.draft.clauses.begin() +
                                           static_cast<std::ptrdiff_t>(index));
                ImGui::PopStyleColor(3);
                ImGui::PopID();
                continue;
              }
              ImGui::PopStyleColor(3);
              DrawHoverDescription("conditions:editor:remove:" +
                                       std::to_string(index),
                                   "Remove this clause from the condition.");
            }

            if (const auto *table = ImGui::GetCurrentTable(); table != nullptr) {
              const auto firstCellRect = ImGui::TableGetCellBgRect(table, 0);
              const auto lastCellRect = ImGui::TableGetCellBgRect(table, 7);
              const auto rowDropRect =
                  ImRect(ImVec2(tableOuterRect.Min.x, firstCellRect.Min.y),
                         ImVec2(tableOuterRect.Max.x, lastCellRect.Max.y));
              const bool insertAfter =
                  ImGui::GetIO().MousePos.y >
                  ((rowDropRect.Min.y + rowDropRect.Max.y) * 0.5f);
              if (reorderPreviewActive &&
                  ImGui::IsMouseHoveringRect(rowDropRect.Min, rowDropRect.Max,
                                             false)) {
                hoveredClauseReorderIndex = static_cast<int>(index);
                hoveredClauseInsertAfter = insertAfter;
                insertionLineY = insertAfter ? rowDropRect.Max.y : rowDropRect.Min.y;
                insertionLineX1 = table->OuterRect.Min.x + 2.0f;
                insertionLineX2 = table->OuterRect.Max.x - 2.0f;
              }
            }

            ImGui::PopID();
            ++index;
          }

          if (reorderPreviewActive && insertionLineY >= 0.0f &&
              insertionLineX2 > insertionLineX1) {
            auto *drawList = ImGui::GetWindowDrawList();
            if (const auto *table = ImGui::GetCurrentTable(); table != nullptr) {
              drawList->PushClipRect(table->OuterRect.Min, table->OuterRect.Max,
                                     false);
              drawList->AddLine(
                  ImVec2(insertionLineX1, insertionLineY),
                  ImVec2(insertionLineX2, insertionLineY),
                  ThemeConfig::GetSingleton()->GetColorU32("PRIMARY"), 2.0f);
              drawList->PopClipRect();
            }
          }

          ImGui::EndTable();
          if (hoveredClauseReorderIndex >= 0 &&
              ImGui::BeginDragDropTargetCustom(
                  tableOuterRect,
                  ImGui::GetID("##condition-clause-reorder-target"))) {
            if (const auto *payload = ImGui::AcceptDragDropPayload(
                    kConditionClausePayloadType,
                    ImGuiDragDropFlags_AcceptNoDrawDefaultRect);
                payload && payload->Data != nullptr &&
                payload->DataSize == sizeof(int)) {
              acceptedSourceClauseIndex = static_cast<std::size_t>(
                  *static_cast<const int *>(payload->Data));
              acceptedInsertAfter = hoveredClauseInsertAfter;
            }
            ImGui::EndDragDropTarget();
          }
          if (acceptedSourceClauseIndex && hoveredClauseReorderIndex >= 0) {
            MoveConditionClause(editor.draft.clauses, *acceptedSourceClauseIndex,
                                static_cast<std::size_t>(hoveredClauseReorderIndex),
                                acceptedInsertAfter);
          }
        }
      }
      ImGui::EndChild();
    }
    ImGui::EndChild();

    if (ImGui::Button("Add Clause")) {
      editor.draft.clauses.push_back(BuildDefaultPlayerClause());
    }
    DrawHoverDescription("conditions:editor:add-clause",
                         "Append another clause to this condition.");

    const auto draftValidationError =
        ValidateConditionDraft(editor.draft, conditions_);
    if (draftValidationError.empty()) {
      editor.error.clear();
    }
    const bool showInlineError =
        !editor.error.empty() && draftValidationError.empty();

    if (showInlineError) {
      ImGui::Spacing();
      ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 205, 205, 255));
      ImGui::TextWrapped("%s", editor.error.c_str());
      ImGui::PopStyleColor();
    }

    ImGui::Spacing();
    ImGui::BeginDisabled(!draftValidationError.empty());
    if (ImGui::Button("Save")) {
      if (SaveConditionEditor(editor)) {
        editor.open = false;
      }
    }
    ImGui::EndDisabled();
    if (!draftValidationError.empty()) {
      DrawHoverDescription(
          "conditions:editor:save-disabled:" +
              std::to_string((std::max)(editor.windowSlot, 1)),
          draftValidationError, 0.2f, ImGuiHoveredFlags_AllowWhenDisabled);
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      editor.error.clear();
      editor.open = false;
    }

    ImGui::End();
  }

  conditionEditors_.erase(
      std::remove_if(
          conditionEditors_.begin(), conditionEditors_.end(),
          [](const ConditionEditorState &a_editor) { return !a_editor.open; }),
      conditionEditors_.end());
  if (conditionEditors_.empty()) {
    focusedConditionEditorWindowSlot_ = 0;
  } else if (focusedConditionEditorWindowSlot_ > 0 &&
             std::ranges::find(conditionEditors_,
                               focusedConditionEditorWindowSlot_,
                               &ConditionEditorState::windowSlot) ==
                 conditionEditors_.end()) {
    focusedConditionEditorWindowSlot_ = 0;
  }
}
} // namespace sosr
