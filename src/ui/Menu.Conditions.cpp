#include "Menu.h"

#include "ConditionMaterializer.h"
#include "conditions/Defaults.h"
#include "conditions/Validation.h"
#include "RE/C/CommandTable.h"
#include "imgui_internal.h"
#include "ui/ConditionFunctionMetadata.h"
#include "ui/ConditionParamOptionCache.h"
#include "ui/conditions/EditorSupport.h"
#include "ui/conditions/Widgets.h"
#include "ui/components/EditableCombo.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cfloat>
#include <cstdio>
#include <functional>
#include <iomanip>
#include <span>
#include <sstream>
#include <string_view>

namespace sosr {
namespace {
using ConditionClause = ui::conditions::Clause;
using ConditionColor = ui::conditions::Color;
using ConditionComparator = ui::conditions::Comparator;
using ConditionConnective = ui::conditions::Connective;
using ConditionDefinition = ui::conditions::Definition;
using ConditionFunctionInfo = ui::condition_editor::FunctionInfo;
using ConditionValueEditorKind = ui::condition_editor::ValueEditorKind;
using ui::condition_widgets::DrawConditionColorSwatch;
using ui::condition_widgets::DrawHoverDescription;
using ui::condition_widgets::MeasureConditionRowHeight;
using ui::condition_editor::BuildConditionFunctionNames;
using ui::condition_editor::BuildNewConditionTemplate;
using ui::condition_editor::BuildSuggestedConditionName;
using ui::condition_editor::CompareTextInsensitive;
using ui::condition_editor::DrawConditionParamEditor;
using ui::condition_editor::DrawNumericClauseValueEditor;
using ui::condition_editor::FindConditionFunctionInfo;
using ui::condition_editor::FormatNumberString;
using ui::condition_editor::GetConditionFunctionInfos;
using ui::condition_editor::GetEditorKindForParamType;
using ui::condition_editor::IsBooleanComparator;
using ui::condition_editor::ParseBooleanComparand;
using ui::condition_editor::PickDistinctConditionColor;
using ui::condition_editor::ResolveEditorParamType;
using ui::condition_editor::ResolveClauseDisplayName;
using ui::condition_editor::ResolveConditionFunctionInfo;
using ui::condition_editor::TrimText;
using ui::condition_editor::ValidateConditionDraft;

constexpr char kIconTrash[] = "\xee\x86\x8c";        // ICON_LC_TRASH
constexpr char kIconGripVertical[] = "\xee\x83\xae"; // ICON_LC_GRIP_VERTICAL
constexpr char kConditionClausePayloadType[] = "SVS_CONDITION_CLAUSE";

void DrawClauseDragHandle(const char *a_id, const ImVec2 a_size) {
  ImGui::InvisibleButton(a_id, a_size);

  const auto min = ImGui::GetItemRectMin();
  const auto max = ImGui::GetItemRectMax();
  const auto color = ImGui::GetColorU32(
      ImGui::IsItemHovered() ? ImGuiCol_Text : ImGuiCol_TextDisabled);
  auto *drawList = ImGui::GetWindowDrawList();
  const auto iconSize = ImGui::CalcTextSize(kIconGripVertical);
  drawList->AddText(ImVec2(min.x + ((max.x - min.x) - iconSize.x) * 0.5f,
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

void MoveConditionDefinition(std::vector<ConditionDefinition> &a_conditions,
                             const std::size_t a_sourceIndex,
                             const std::size_t a_targetIndex,
                             const bool a_insertAfter) {
  if (a_sourceIndex >= a_conditions.size() ||
      a_targetIndex >= a_conditions.size()) {
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

  auto condition = std::move(a_conditions[a_sourceIndex]);
  a_conditions.erase(a_conditions.begin() +
                     static_cast<std::ptrdiff_t>(a_sourceIndex));
  a_conditions.insert(a_conditions.begin() +
                          static_cast<std::ptrdiff_t>(destinationIndex),
                      std::move(condition));
}

void MoveConditionDefinitionToSlot(std::vector<ConditionDefinition> &a_conditions,
                                   const std::size_t a_sourceIndex,
                                   std::size_t a_slotIndex) {
  if (a_sourceIndex >= a_conditions.size() || a_slotIndex > a_conditions.size()) {
    return;
  }

  auto condition = std::move(a_conditions[a_sourceIndex]);
  a_conditions.erase(a_conditions.begin() +
                     static_cast<std::ptrdiff_t>(a_sourceIndex));
  if (a_sourceIndex < a_slotIndex) {
    --a_slotIndex;
  }
  a_conditions.insert(a_conditions.begin() +
                          static_cast<std::ptrdiff_t>(a_slotIndex),
                      std::move(condition));
}

void CopyTextToBuffer(const std::string &a_text, char *a_buffer,
                      const std::size_t a_bufferSize) {
  if (a_bufferSize == 0) {
    return;
  }

  std::snprintf(a_buffer, a_bufferSize, "%s", a_text.c_str());
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

const char *ConnectiveLabel(const ConditionConnective a_connective) {
  return a_connective == ConditionConnective::Or ? "OR" : "AND";
}

void DrawClauseHeaderCell(const char *a_id, const char *a_label,
                          const std::string_view a_tooltip) {
  ImGui::TextDisabled("%s", a_label);
  DrawHoverDescription(a_id, a_tooltip);
}

struct ConditionDeleteUsage {
  std::size_t referencingConditionCount{0};
  std::size_t appliedRowCount{0};

  [[nodiscard]] bool CanDelete() const {
    return referencingConditionCount == 0 && appliedRowCount == 0;
  }

  [[nodiscard]] std::string BuildTooltip() const {
    if (CanDelete()) {
      return {};
    }

    if (referencingConditionCount != 0 && appliedRowCount != 0) {
      return "Condition is in use by other conditions and applied to one or "
             "more workbench rows.";
    }
    if (referencingConditionCount != 0) {
      return "Condition is in use by other conditions.";
    }
    return "Condition is applied to one or more workbench rows.";
  }
};
} // namespace

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
        ((!editor.error.empty())
             ? (ImGui::GetTextLineHeightWithSpacing() * 2.0f)
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

      auto color = editor.draft.color;
      ImGui::SetNextItemWidth(
          (std::min)(260.0f, ImGui::GetContentRegionAvail().x));
      if (ImGui::ColorEdit3("##color", &color.x,
                            ImGuiColorEditFlags_DisplayRGB)) {
        color.w = 1.0f;
        editor.draft.color = color;
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
      const auto actionsColumnWidth = editButtonWidth + style.ItemSpacing.x +
                                      deleteButtonWidth +
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
          DrawHoverDescription("conditions:help:move",
                               "Drag to reorder clauses.");
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
            const auto *functionInfo = ResolveConditionFunctionInfo(
                clause, conditions_, customFunctionInfo);
            auto selectedFunctionName =
                functionInfo ? functionInfo->name : clause.functionName;

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            DrawClauseDragHandle("##drag-handle",
                                 ImVec2(ImGui::GetContentRegionAvail().x,
                                        ImGui::GetFrameHeight()));
            DrawHoverDescription("conditions:editor:drag:" +
                                     std::to_string(index),
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
                    : conditions::FindDefinitionById(conditions_,
                                                     clause.customConditionId);
            float functionWidth = ImGui::GetContentRegionAvail().x;
            if (customCondition != nullptr) {
              const float swatchSize = ImGui::GetFrameHeight() - 2.0f;
              const float spacing = ImGui::GetStyle().ItemSpacing.x;
              DrawConditionColorSwatch("##custom-condition-color",
                                       customCondition->color,
                                       "Referenced custom condition color: " +
                                           customCondition->name);
              ImGui::SameLine(0.0f, spacing);
              functionWidth =
                  (std::max)(0.0f, functionWidth - swatchSize - spacing);
            }
            if (ui::components::DrawSearchableDropdown(
                    "##function", "Condition function", selectedFunctionName,
                    conditionFunctionNames, functionWidth)) {
              clause.arguments[0].clear();
              clause.arguments[1].clear();
              if (const auto *selectedCustomCondition =
                      conditions::FindDefinitionByName(
                          conditions_, selectedFunctionName,
                          editor.sourceConditionId);
                  selectedCustomCondition != nullptr) {
                clause.customConditionId = selectedCustomCondition->id;
                clause.functionName.clear();
              } else {
                clause.customConditionId.clear();
                clause.functionName = selectedFunctionName;
              }
              customFunctionInfo.reset();
              functionInfo = ResolveConditionFunctionInfo(clause, conditions_,
                                                          customFunctionInfo);
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
                ImGui::InputText(paramIndex == 0 ? "##arg1" : "##arg2", buffer,
                                 sizeof(buffer), ImGuiInputTextFlags_ReadOnly);
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
                          kBooleanComparators.begin(),
                          kBooleanComparators.end())
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
                    ? static_cast<int>(std::distance(
                          availableComparators.begin(), comparatorIt))
                    : 0;
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            if (ImGui::BeginCombo(
                    "##comparator",
                    ComparatorLabel(availableComparators[comparatorIndex]))) {
              for (int optionIndex = 0;
                   optionIndex < static_cast<int>(availableComparators.size());
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
              bool boolComparand =
                  ParseBooleanComparand(clause.comparand, true);
              if (ImGui::Checkbox("##comparand", &boolComparand)) {
                clause.comparand = boolComparand ? "1" : "0";
              } else if (clause.comparand.empty() ||
                         clause.comparand != "0" && clause.comparand != "1") {
                clause.comparand = boolComparand ? "1" : "0";
              }
            } else {
              DrawNumericClauseValueEditor("##comparand", clause.comparand,
                                           ConditionValueEditorKind::Number,
                                           ImGui::GetContentRegionAvail().x);
            }
            DrawHoverDescription(
                "conditions:editor:value:" + std::to_string(index),
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
              if (ImGui::Button(kIconTrash, ImVec2(deleteButtonWidth, 0.0f))) {
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

            if (const auto *table = ImGui::GetCurrentTable();
                table != nullptr) {
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
                insertionLineY =
                    insertAfter ? rowDropRect.Max.y : rowDropRect.Min.y;
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
            if (const auto *table = ImGui::GetCurrentTable();
                table != nullptr) {
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
            MoveConditionClause(
                editor.draft.clauses, *acceptedSourceClauseIndex,
                static_cast<std::size_t>(hoveredClauseReorderIndex),
                acceptedInsertAfter);
          }
        }
      }
      ImGui::EndChild();
    }
    ImGui::EndChild();

    if (ImGui::Button("Add Clause")) {
      editor.draft.clauses.push_back(conditions::BuildDefaultPlayerClause());
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
      DrawHoverDescription("conditions:editor:save-disabled:" +
                               std::to_string((std::max)(editor.windowSlot, 1)),
                           draftValidationError, 0.2f,
                           ImGuiHoveredFlags_AllowWhenDisabled);
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
