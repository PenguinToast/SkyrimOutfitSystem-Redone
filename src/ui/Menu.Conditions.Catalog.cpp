#include "Menu.h"

#include "ConditionMaterializer.h"
#include "ThemeConfig.h"
#include "imgui_internal.h"
#include "ui/conditions/Widgets.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <optional>

namespace sosr {
namespace {
using ConditionClause = ui::conditions::Clause;
using ConditionDefinition = ui::conditions::Definition;

void MoveConditionDefinitionToSlot(
    std::vector<ConditionDefinition> &a_conditions,
    const std::size_t a_sourceIndex, std::size_t a_slotIndex) {
  if (a_sourceIndex >= a_conditions.size() ||
      a_slotIndex > a_conditions.size()) {
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

constexpr char kIconTrash[] = "\xee\x86\x8c"; // ICON_LC_TRASH
} // namespace

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
  std::optional<std::size_t> pendingDeleteIndex;
  std::vector<ImRect> conditionRowRects;
  conditionRowRects.reserve(conditions_.size());

  for (std::size_t index = 0; index < conditions_.size(); ++index) {
    const auto &condition = conditions_[index];
    ConditionDeleteUsage deleteUsage;
    for (const auto &otherCondition : conditions_) {
      if (otherCondition.id == condition.id) {
        continue;
      }
      if (std::ranges::any_of(
              otherCondition.clauses, [&](const ConditionClause &a_clause) {
                return a_clause.customConditionId == condition.id;
              })) {
        ++deleteUsage.referencingConditionCount;
      }
    }
    for (const auto &row : workbench_.GetRows()) {
      if (row.conditionId && *row.conditionId == condition.id) {
        ++deleteUsage.appliedRowCount;
      }
    }
    const auto deleteTooltip = deleteUsage.BuildTooltip();
    const bool deleteEnabled = deleteUsage.CanDelete();

    const auto rowWrapWidth =
        (std::max)(ImGui::GetContentRegionAvail().x - 52.0f, 120.0f);
    const auto rowHeight = ui::condition_widgets::MeasureConditionRowHeight(
        condition, rowWrapWidth);

    ImGui::TableNextRow(0, rowHeight);
    ImGui::TableSetColumnIndex(0);
    ImGui::PushID(static_cast<int>(index));

    const auto width = ImGui::GetContentRegionAvail().x;
    ImGui::InvisibleButton("##condition-row", ImVec2(width, rowHeight));
    const auto min = ImGui::GetItemRectMin();
    const auto max = ImGui::GetItemRectMax();
    const auto stripeWidth = 6.0f;
    const auto deletePaneWidth = 34.0f;
    const auto rounding = ImGui::GetStyle().FrameRounding;
    const ImVec2 deleteMin(max.x - deletePaneWidth, min.y);
    const ImVec2 deleteMax = max;
    const bool deleteHovered =
        ImGui::IsMouseHoveringRect(deleteMin, deleteMax, false);
    const auto hovered = ImGui::IsItemHovered() || deleteHovered;
    const bool rowBodyHovered =
        hovered && !deleteHovered &&
        ImGui::IsMouseHoveringRect(min, ImVec2(deleteMin.x, max.y), false);
    rowClicked |= rowBodyHovered && ImGui::IsItemClicked(ImGuiMouseButton_Left);
    if (rowBodyHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
      OpenConditionEditorDialog(index);
    }

    auto *drawList = ImGui::GetWindowDrawList();
    const auto bodyColor =
        hovered ? IM_COL32(42, 42, 44, 240) : IM_COL32(34, 34, 36, 225);
    drawList->AddRectFilled(min, max, bodyColor, rounding);
    drawList->AddRect(
        min, max,
        ImGui::GetColorU32(ImVec4(condition.color.x, condition.color.y,
                                  condition.color.z, 0.75f)),
        rounding);
    drawList->AddRectFilled(
        min, ImVec2(min.x + stripeWidth, max.y),
        ImGui::GetColorU32(ui::conditions::ToImGuiColor(condition.color)),
        rounding,
        ImDrawFlags_RoundCornersTopLeft | ImDrawFlags_RoundCornersBottomLeft);

    auto *theme = ThemeConfig::GetSingleton();
    const ImU32 deleteFillColor =
        deleteEnabled
            ? (deleteHovered ? theme->GetColorU32("DECLINE", 0.95f)
                             : theme->GetColorU32("DECLINE", 0.78f))
            : theme->GetColorU32("DECLINE", deleteHovered ? 0.35f : 0.24f);
    drawList->AddRectFilled(deleteMin, deleteMax, deleteFillColor, rounding,
                            ImDrawFlags_RoundCornersTopRight |
                                ImDrawFlags_RoundCornersBottomRight);
    drawList->AddLine(ImVec2(deleteMin.x, deleteMin.y),
                      ImVec2(deleteMin.x, deleteMax.y),
                      IM_COL32(255, 255, 255, 18), 1.0f);
    const auto deleteIconSize = ImGui::CalcTextSize(kIconTrash);
    drawList->AddText(
        ImVec2(deleteMin.x + ((deletePaneWidth - deleteIconSize.x) * 0.5f),
               deleteMin.y +
                   (((deleteMax.y - deleteMin.y) - deleteIconSize.y) * 0.5f)),
        ImGui::GetColorU32(deleteEnabled ? ImGuiCol_Text
                                         : ImGuiCol_TextDisabled),
        kIconTrash);

    const auto contentMin = ImVec2(min.x + stripeWidth + 10.0f,
                                   min.y + ImGui::GetStyle().FramePadding.y);
    const auto textColor = ImGui::GetColorU32(
        ImVec4(condition.color.x, condition.color.y, condition.color.z, 1.0f));
    const auto clipRect =
        ImVec4(contentMin.x, min.y, deleteMin.x - 8.0f, max.y);
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

    if (deleteHovered) {
      if (deleteEnabled) {
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
          pendingDeleteIndex = index;
        }
      } else if (!deleteTooltip.empty()) {
        ui::condition_widgets::DrawHoverDescription(
            "conditions:delete-disabled:" + condition.id, deleteTooltip, 0.2f);
      }
    }

    if (!deleteHovered && ImGui::BeginDragDropSource()) {
      DraggedConditionPayload payload{};
      std::snprintf(payload.conditionId.data(), payload.conditionId.size(),
                    "%s", condition.id.c_str());
      ImGui::SetDragDropPayload("SVS_CONDITION", &payload, sizeof(payload));
      ImGui::TextUnformatted(condition.name.c_str());
      if (!condition.description.empty()) {
        ImGui::TextUnformatted(condition.description.c_str());
      }
      ImGui::EndDragDropSource();
    }

    conditionRowRects.emplace_back(min, max);

    ImGui::PopID();
  }

  ImGui::EndTable();

  std::optional<std::size_t> hoveredInsertionSlot;
  ImRect hoveredInsertionRect;
  float insertionLineY = -1.0f;
  float insertionLineXMin = 0.0f;
  float insertionLineXMax = 0.0f;

  if (ImGui::IsDragDropActive() && !conditionRowRects.empty()) {
    const float fallbackBandHalfHeight =
        (std::max)(6.0f, ImGui::GetStyle().ItemSpacing.y * 0.5f);
    const auto xMin = conditionRowRects.front().Min.x;
    const auto xMax = conditionRowRects.front().Max.x;
    std::vector<float> insertionLineYs;
    insertionLineYs.reserve(conditionRowRects.size() + 1);
    insertionLineYs.push_back(conditionRowRects.front().Min.y -
                              fallbackBandHalfHeight);
    for (std::size_t index = 0; index + 1 < conditionRowRects.size(); ++index) {
      const auto &currentRect = conditionRowRects[index];
      const auto &nextRect = conditionRowRects[index + 1];
      insertionLineYs.push_back((currentRect.Max.y + nextRect.Min.y) * 0.5f);
    }
    insertionLineYs.push_back(conditionRowRects.back().Max.y +
                              fallbackBandHalfHeight);

    for (std::size_t slotIndex = 0; slotIndex < insertionLineYs.size();
         ++slotIndex) {
      const float lineY = insertionLineYs[slotIndex];
      const float bandMinY =
          slotIndex == 0 ? (lineY - fallbackBandHalfHeight)
                         : ((insertionLineYs[slotIndex - 1] + lineY) * 0.5f);
      const float bandMaxY =
          slotIndex + 1 == insertionLineYs.size()
              ? (lineY + fallbackBandHalfHeight)
              : ((lineY + insertionLineYs[slotIndex + 1]) * 0.5f);
      const ImRect slotRect(ImVec2(xMin, bandMinY), ImVec2(xMax, bandMaxY));
      if (!ImGui::IsMouseHoveringRect(slotRect.Min, slotRect.Max, false)) {
        continue;
      }

      hoveredInsertionSlot = slotIndex;
      hoveredInsertionRect = slotRect;
      insertionLineY = lineY;
      insertionLineXMin = xMin;
      insertionLineXMax = xMax;
      break;
    }
  }

  if (insertionLineY >= 0.0f && insertionLineXMax > insertionLineXMin) {
    ImGui::GetWindowDrawList()->AddLine(
        ImVec2(insertionLineXMin, insertionLineY),
        ImVec2(insertionLineXMax, insertionLineY),
        ThemeConfig::GetSingleton()->GetColorU32("PRIMARY"), 2.0f);
  }

  if (hoveredInsertionSlot &&
      ImGui::BeginDragDropTargetCustom(
          hoveredInsertionRect,
          ImGui::GetID(("##condition-reorder-slot-" +
                        std::to_string(*hoveredInsertionSlot))
                           .c_str()))) {
    if (const auto *payload = ImGui::AcceptDragDropPayload(
            "SVS_CONDITION", ImGuiDragDropFlags_AcceptNoDrawDefaultRect);
        payload && payload->DataSize == sizeof(DraggedConditionPayload)) {
      DraggedConditionPayload dragPayload{};
      std::memcpy(&dragPayload, payload->Data, sizeof(dragPayload));
      if (const auto it = std::ranges::find(
              conditions_, std::string_view(dragPayload.conditionId.data()),
              &ConditionDefinition::id);
          it != conditions_.end()) {
        const auto sourceIndex =
            static_cast<std::size_t>(std::distance(conditions_.begin(), it));
        MoveConditionDefinitionToSlot(conditions_, sourceIndex,
                                      *hoveredInsertionSlot);
      }
    }
    ImGui::EndDragDropTarget();
  }

  if (pendingDeleteIndex && *pendingDeleteIndex < conditions_.size()) {
    const auto deletedConditionId = conditions_[*pendingDeleteIndex].id;
    conditions_.erase(conditions_.begin() +
                      static_cast<std::ptrdiff_t>(*pendingDeleteIndex));
    sosr::conditions::RebuildConditionDependencyMetadata(conditions_);
    sosr::conditions::InvalidateConditionMaterializationCaches(conditions_);
    for (auto &editor : conditionEditors_) {
      if (editor.sourceConditionId == deletedConditionId) {
        editor.error.clear();
        editor.open = false;
      }
    }
  }

  return rowClicked;
}
} // namespace sosr
