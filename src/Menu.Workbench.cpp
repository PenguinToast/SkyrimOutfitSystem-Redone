#include "Menu.h"

#include "imgui_internal.h"

#include <cstring>
#include <optional>
#include <unordered_map>

namespace {
constexpr char kVariantItemPayloadType[] = "SOSNG_VARIANT_ITEM";

struct ActiveWorkbenchVisual {
  std::string widgetId;
  std::string description;
  std::uint64_t slotMask{0};
  int rowIndex{-1};
};

struct OverrideConflictInfo {
  std::vector<std::string> targetWidgetIds;
  std::vector<std::string> targetDescriptions;
};

std::string DescribeActiveWorkbenchVisual(
    const sosng::workbench::VariantWorkbenchRow &a_row,
    const sosng::workbench::EquipmentWidgetItem &a_item, bool a_isOverride) {
  if (a_isOverride) {
    return a_item.name + " override on " + a_row.equipped.name;
  }

  return a_row.equipped.name + " (equipped)";
}
} // namespace

namespace sosng {
bool Menu::DrawEquipmentInfoWidget(const char *a_id,
                                   const workbench::EquipmentWidgetItem &a_item,
                                   bool a_allowDrag,
                                   DragSourceKind a_sourceKind,
                                   bool a_showDeleteButton, int a_rowIndex,
                                   int a_itemIndex, bool a_conflict) {
  ImGui::PushID(a_id);

  constexpr float paddingX = 10.0f;
  constexpr float paddingY = 7.0f;
  const auto lineHeight = ImGui::GetTextLineHeight();
  const auto frameHeight = paddingY * 2.0f + lineHeight * 2.0f + 4.0f;
  const auto width = ImGui::GetContentRegionAvail().x;
  const auto size = ImVec2(width > 0.0f ? width : 1.0f, frameHeight);
  bool deleteClicked = false;

  ImGui::InvisibleButton("##equipment-widget", size);

  const auto rectMin = ImGui::GetItemRectMin();
  const auto rectMax = ImGui::GetItemRectMax();
  const auto hovered = ImGui::IsItemHovered();
  const auto active = ImGui::IsItemActive();
  auto *drawList = ImGui::GetWindowDrawList();

  ImU32 fillColor = IM_COL32(28, 33, 41, 242);
  ImU32 borderColor = IM_COL32(85, 103, 122, 255);
  if (a_conflict) {
    fillColor = IM_COL32(76, 24, 24, 242);
    borderColor = IM_COL32(192, 84, 84, 255);
  }
  if (active) {
    fillColor =
        a_conflict ? IM_COL32(112, 36, 36, 255) : IM_COL32(45, 63, 86, 255);
  } else if (hovered) {
    fillColor =
        a_conflict ? IM_COL32(94, 32, 32, 250) : IM_COL32(37, 47, 60, 250);
  }

  drawList->AddRectFilled(rectMin, rectMax, fillColor, 8.0f);
  drawList->AddRect(rectMin, rectMax, borderColor, 8.0f);

  const auto namePos = ImVec2(rectMin.x + paddingX, rectMin.y + paddingY);
  const auto slotPos =
      ImVec2(rectMin.x + paddingX, rectMin.y + paddingY + lineHeight + 4.0f);
  const auto clipMin = ImVec2(rectMin.x + paddingX, rectMin.y + paddingY);
  const auto buttonWidth = a_showDeleteButton ? 24.0f : 0.0f;
  const auto clipMax =
      ImVec2(rectMax.x - paddingX - buttonWidth, rectMax.y - paddingY);
  const auto buttonSize = ImVec2(20.0f, 20.0f);
  const auto buttonMin =
      ImVec2(rectMax.x - paddingX - buttonSize.x, rectMin.y + paddingY);
  const auto buttonMax =
      ImVec2(buttonMin.x + buttonSize.x, buttonMin.y + buttonSize.y);
  const bool deleteHovered =
      a_showDeleteButton && ImGui::IsMouseHoveringRect(buttonMin, buttonMax);

  drawList->PushClipRect(clipMin, clipMax, true);
  drawList->AddText(namePos, IM_COL32(235, 239, 244, 255), a_item.name.c_str());
  drawList->AddText(slotPos, IM_COL32(161, 188, 214, 255),
                    a_item.slotText.c_str());
  drawList->PopClipRect();

  if (a_showDeleteButton) {
    const auto deleteFill =
        deleteHovered ? IM_COL32(122, 52, 52, 255) : IM_COL32(88, 43, 43, 235);
    drawList->AddRectFilled(buttonMin, buttonMax, deleteFill, 4.0f);
    drawList->AddRect(buttonMin, buttonMax, IM_COL32(160, 92, 92, 255), 4.0f);
    drawList->AddText(ImVec2(buttonMin.x + 6.0f, buttonMin.y + 2.0f),
                      IM_COL32(244, 232, 232, 255), "X");

    if (deleteHovered && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
      deleteClicked = true;
    }
  }

  if (a_allowDrag && !deleteHovered && ImGui::BeginDragDropSource()) {
    DraggedEquipmentPayload payload{};
    payload.sourceKind = static_cast<std::uint32_t>(a_sourceKind);
    payload.rowIndex = a_rowIndex;
    payload.itemIndex = a_itemIndex;
    payload.formID = a_item.formID;
    ImGui::SetDragDropPayload(kVariantItemPayloadType, &payload,
                              sizeof(payload));
    ImGui::TextUnformatted(a_item.name.c_str());
    ImGui::Text("%s", a_item.slotText.c_str());
    ImGui::EndDragDropSource();
  }

  ImGui::PopID();
  return deleteClicked;
}

void Menu::AcceptOverridePayload(int a_targetRowIndex) {
  const auto *payload = ImGui::AcceptDragDropPayload(kVariantItemPayloadType);
  if (!payload || payload->DataSize != sizeof(DraggedEquipmentPayload)) {
    return;
  }

  DraggedEquipmentPayload dragPayload{};
  std::memcpy(&dragPayload, payload->Data, sizeof(dragPayload));

  if (dragPayload.sourceKind ==
      static_cast<std::uint32_t>(DragSourceKind::Override)) {
    workbench_.MoveOverride(dragPayload.rowIndex, dragPayload.itemIndex,
                            a_targetRowIndex);
  } else if (dragPayload.sourceKind ==
                 static_cast<std::uint32_t>(DragSourceKind::Catalog) ||
             dragPayload.sourceKind ==
                 static_cast<std::uint32_t>(DragSourceKind::Row)) {
    workbench_.AddCatalogOverride(a_targetRowIndex, dragPayload.formID);
  }
}

void Menu::ApplyRowReorder(const DraggedEquipmentPayload &a_dragPayload,
                           int a_targetRowIndex, bool a_insertAfter) {
  if (a_dragPayload.sourceKind ==
      static_cast<std::uint32_t>(DragSourceKind::Row)) {
    workbench_.ApplyRowReorder(a_dragPayload.rowIndex, a_targetRowIndex,
                               a_insertAfter);
  } else if (a_dragPayload.sourceKind ==
             static_cast<std::uint32_t>(DragSourceKind::Catalog)) {
    workbench_.InsertCatalogRow(a_dragPayload.formID, a_targetRowIndex,
                                a_insertAfter);
  }
}

void Menu::AcceptOverrideDeletePayload() {
  const auto *payload = ImGui::AcceptDragDropPayload(kVariantItemPayloadType);
  if (!payload || payload->DataSize != sizeof(DraggedEquipmentPayload)) {
    return;
  }

  DraggedEquipmentPayload dragPayload{};
  std::memcpy(&dragPayload, payload->Data, sizeof(dragPayload));
  if (dragPayload.sourceKind ==
      static_cast<std::uint32_t>(DragSourceKind::Override)) {
    workbench_.DeleteOverride(dragPayload.rowIndex, dragPayload.itemIndex);
  } else if (dragPayload.sourceKind ==
             static_cast<std::uint32_t>(DragSourceKind::Row)) {
    workbench_.DeleteRow(dragPayload.rowIndex);
  }
}

void Menu::DrawVariantWorkbenchPane() {
  ImGui::TextUnformatted("Variant workbench");
  ImGui::Separator();

  const auto &rows = workbench_.GetRows();
  if (rows.empty()) {
    ImGui::TextWrapped("No equipped armor pieces were found on the player.");
    return;
  }

  std::vector<ActiveWorkbenchVisual> activeVisuals;
  activeVisuals.reserve(rows.size() * 2);
  for (int rowIndex = 0; rowIndex < static_cast<int>(rows.size()); ++rowIndex) {
    const auto &row = rows[static_cast<std::size_t>(rowIndex)];
    if (!row.isEquipped) {
      continue;
    }

    if (!row.overrides.empty()) {
      for (int overrideIndex = 0;
           overrideIndex < static_cast<int>(row.overrides.size());
           ++overrideIndex) {
        const auto &item =
            row.overrides[static_cast<std::size_t>(overrideIndex)];
        activeVisuals.push_back(
            {.widgetId = "override:" + std::to_string(rowIndex) + ":" +
                         std::to_string(overrideIndex),
             .description = DescribeActiveWorkbenchVisual(row, item, true),
             .slotMask = item.slotMask,
             .rowIndex = rowIndex});
      }
    } else if (!row.hideEquipped) {
      activeVisuals.push_back({.widgetId = row.key,
                               .description = DescribeActiveWorkbenchVisual(
                                   row, row.equipped, false),
                               .slotMask = row.equipped.slotMask,
                               .rowIndex = rowIndex});
    }
  }

  std::unordered_map<std::string, OverrideConflictInfo> overrideConflicts;
  overrideConflicts.reserve(rows.size() * 2);
  for (int rowIndex = 0; rowIndex < static_cast<int>(rows.size()); ++rowIndex) {
    const auto &row = rows[static_cast<std::size_t>(rowIndex)];
    for (int overrideIndex = 0;
         overrideIndex < static_cast<int>(row.overrides.size());
         ++overrideIndex) {
      const auto &item = row.overrides[static_cast<std::size_t>(overrideIndex)];
      OverrideConflictInfo info{};
      for (const auto &activeVisual : activeVisuals) {
        if (activeVisual.rowIndex == rowIndex ||
            (activeVisual.slotMask & item.slotMask) == 0) {
          continue;
        }

        info.targetWidgetIds.push_back(activeVisual.widgetId);
        info.targetDescriptions.push_back(activeVisual.description + " [" +
                                          item.slotText + "]");
      }

      if (!info.targetWidgetIds.empty()) {
        overrideConflicts.emplace("override:" + std::to_string(rowIndex) + ":" +
                                      std::to_string(overrideIndex),
                                  std::move(info));
      }
    }
  }

  constexpr float deletePaneHeight = 86.0f;
  const auto tableHeight =
      (std::max)(0.0f, ImGui::GetContentRegionAvail().y - deletePaneHeight);
  if (ImGui::BeginChild("##variant-workbench-scroll", ImVec2(0.0f, tableHeight),
                        ImGuiChildFlags_None)) {
    if (ImGui::BeginTable("##variant-workbench", 3,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_Resizable |
                              ImGuiTableFlags_ScrollY,
                          ImVec2(0.0f, 0.0f))) {
      ImGui::TableSetupColumn("Equipped", ImGuiTableColumnFlags_WidthStretch,
                              0.80f);
      ImGui::TableSetupColumn("Overrides", ImGuiTableColumnFlags_WidthStretch,
                              1.05f);
      ImGui::TableSetupColumn("Hide",
                              ImGuiTableColumnFlags_WidthFixed |
                                  ImGuiTableColumnFlags_NoResize,
                              72.0f);
      ImGui::TableSetupScrollFreeze(0, 1);
      ImGui::TableHeadersRow();

      const auto *activePayload = ImGui::GetDragDropPayload();
      const bool reorderPreviewActive =
          activePayload && activePayload->Data != nullptr &&
          activePayload->IsDataType(kVariantItemPayloadType) &&
          activePayload->DataSize == sizeof(DraggedEquipmentPayload) &&
          ([](const DraggedEquipmentPayload *a_payload) {
            return a_payload->sourceKind ==
                       static_cast<std::uint32_t>(DragSourceKind::Row) ||
                   a_payload->sourceKind ==
                       static_cast<std::uint32_t>(DragSourceKind::Catalog);
          })(static_cast<const DraggedEquipmentPayload *>(activePayload->Data));
      float insertionLineY = -1.0f;
      float insertionLineX1 = -1.0f;
      float insertionLineX2 = -1.0f;
      std::optional<DraggedEquipmentPayload> acceptedRowReorderPayload;
      int acceptedRowReorderIndex = -1;
      bool acceptedRowInsertAfter = false;
      int hoveredRowReorderIndex = -1;
      bool hoveredRowInsertAfter = false;
      std::unordered_map<std::string, ImRect> widgetRects;
      widgetRects.reserve(rows.size() * 3);
      std::string hoveredConflictWidgetId;
      std::vector<float> rowTopY;
      std::vector<float> rowBottomY;
      rowTopY.reserve(rows.size());
      rowBottomY.reserve(rows.size());

      for (int rowIndex = 0; rowIndex < static_cast<int>(rows.size());
           ++rowIndex) {
        const auto overrideCount =
            rows[static_cast<std::size_t>(rowIndex)].overrides.size();
        const auto dropZoneHeight = (std::max)(
            72.0f, 14.0f + static_cast<float>(overrideCount) *
                               (ImGui::GetTextLineHeightWithSpacing() * 2.5f));
        const auto widgetHeight = 18.0f + (ImGui::GetTextLineHeight() * 2.0f);
        const auto rowHeight = (std::max)(widgetHeight, dropZoneHeight);
        ImGui::TableNextRow(ImGuiTableRowFlags_None, rowHeight);
        if (rows[static_cast<std::size_t>(rowIndex)].isEquipped) {
          ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                                 IM_COL32(36, 58, 41, 112));
        }

        ImGui::TableSetColumnIndex(0);
        DrawEquipmentInfoWidget(
            rows[static_cast<std::size_t>(rowIndex)].key.c_str(),
            rows[static_cast<std::size_t>(rowIndex)].equipped, true,
            DragSourceKind::Row, false, rowIndex);
        widgetRects.insert_or_assign(
            rows[static_cast<std::size_t>(rowIndex)].key,
            ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()));
        const auto *table = ImGui::GetCurrentTable();
        if (table) {
          const ImRect leftCellRect = ImGui::TableGetCellBgRect(table, 0);
          rowTopY.push_back(leftCellRect.Min.y);
          const bool insertAfter =
              ImGui::GetIO().MousePos.y >
              ((leftCellRect.Min.y + leftCellRect.Max.y) * 0.5f);

          if (reorderPreviewActive &&
              ImGui::IsMouseHoveringRect(leftCellRect.Min, leftCellRect.Max)) {
            hoveredRowReorderIndex = rowIndex;
            hoveredRowInsertAfter = insertAfter;
            insertionLineX1 = table->OuterRect.Min.x + 2.0f;
            insertionLineX2 = table->OuterRect.Max.x - 2.0f;
          }

          if (ImGui::BeginDragDropTargetCustom(
                  leftCellRect,
                  ImGui::GetID(
                      ("##row-reorder-" + std::to_string(rowIndex)).c_str()))) {
            if (const auto *payload = ImGui::AcceptDragDropPayload(
                    kVariantItemPayloadType,
                    ImGuiDragDropFlags_AcceptNoDrawDefaultRect);
                payload && payload->Data != nullptr &&
                payload->DataSize == sizeof(DraggedEquipmentPayload)) {
              DraggedEquipmentPayload dragPayload{};
              std::memcpy(&dragPayload, payload->Data, sizeof(dragPayload));
              if (dragPayload.sourceKind ==
                      static_cast<std::uint32_t>(DragSourceKind::Row) ||
                  dragPayload.sourceKind ==
                      static_cast<std::uint32_t>(DragSourceKind::Catalog)) {
                acceptedRowReorderPayload = dragPayload;
                acceptedRowReorderIndex = rowIndex;
                acceptedRowInsertAfter = insertAfter;
              }
            }
            ImGui::EndDragDropTarget();
          }
        }

        ImGui::TableSetColumnIndex(1);
        const auto dropId = "##override-drop-" + std::to_string(rowIndex);
        ImGui::BeginChild(dropId.c_str(), ImVec2(0.0f, dropZoneHeight),
                          ImGuiChildFlags_Borders,
                          ImGuiWindowFlags_NoScrollbar);
        const auto childPos = ImGui::GetWindowPos();
        const auto childSize = ImGui::GetWindowSize();

        if (overrideCount == 0) {
          ImGui::TextWrapped("Drop equipment overrides here.");
        } else {
          for (int overrideIndex = 0;
               overrideIndex < static_cast<int>(overrideCount);
               ++overrideIndex) {
            if (overrideIndex > 0) {
              ImGui::Spacing();
            }

            const auto widgetId = "override:" + std::to_string(rowIndex) + ":" +
                                  std::to_string(overrideIndex);
            if (DrawEquipmentInfoWidget(
                    widgetId.c_str(),
                    rows[static_cast<std::size_t>(rowIndex)]
                        .overrides[static_cast<std::size_t>(overrideIndex)],
                    true, DragSourceKind::Override, true, rowIndex,
                    overrideIndex, overrideConflicts.contains(widgetId))) {
              workbench_.DeleteOverride(rowIndex, overrideIndex);
              break;
            }
            widgetRects.insert_or_assign(
                widgetId,
                ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()));
            if (overrideConflicts.contains(widgetId) &&
                ImGui::IsItemHovered()) {
              hoveredConflictWidgetId = widgetId;
              if (ImGui::BeginTooltip()) {
                ImGui::TextWrapped("This override shares equipment slots with "
                                   "currently active visuals.");
                ImGui::Separator();
                for (const auto &description :
                     overrideConflicts.at(widgetId).targetDescriptions) {
                  ImGui::BulletText("%s", description.c_str());
                }
                ImGui::EndTooltip();
              }
            }
          }
        }

        const ImRect dropRect(childPos, ImVec2(childPos.x + childSize.x,
                                               childPos.y + childSize.y));
        if (ImGui::BeginDragDropTargetCustom(
                dropRect, ImGui::GetID(("##override-cell-target-" +
                                        std::to_string(rowIndex))
                                           .c_str()))) {
          AcceptOverridePayload(rowIndex);
          ImGui::EndDragDropTarget();
        }

        ImGui::EndChild();

        ImGui::TableSetColumnIndex(2);
        bool hideEquipped =
            rows[static_cast<std::size_t>(rowIndex)].hideEquipped;
        ImGui::SetCursorPosX(
            ImGui::GetCursorPosX() +
            (ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight()) *
                0.5f);
        if (ImGui::Checkbox(
                ("##hide-equipped-" + std::to_string(rowIndex)).c_str(),
                &hideEquipped)) {
          workbench_.SetHideEquipped(rowIndex, hideEquipped);
        }

        if (const auto *rowTable = ImGui::GetCurrentTable()) {
          const auto bottomRect = ImGui::TableGetCellBgRect(rowTable, 2);
          rowBottomY.push_back(bottomRect.Max.y + rowTable->RowCellPaddingY);
        } else {
          rowBottomY.push_back(ImGui::GetItemRectMax().y);
        }
      }

      if (hoveredRowReorderIndex >= 0 &&
          hoveredRowReorderIndex < static_cast<int>(rowTopY.size()) &&
          hoveredRowReorderIndex < static_cast<int>(rowBottomY.size())) {
        insertionLineY =
            hoveredRowInsertAfter
                ? rowBottomY[static_cast<std::size_t>(hoveredRowReorderIndex)]
                : rowTopY[static_cast<std::size_t>(hoveredRowReorderIndex)];
      }

      if (reorderPreviewActive && insertionLineY >= 0.0f &&
          insertionLineX2 > insertionLineX1) {
        auto *drawList = ImGui::GetWindowDrawList();
        drawList->AddLine(ImVec2(insertionLineX1, insertionLineY),
                          ImVec2(insertionLineX2, insertionLineY),
                          IM_COL32(116, 189, 255, 255), 2.0f);
      }

      if (!hoveredConflictWidgetId.empty()) {
        auto *drawList = ImGui::GetWindowDrawList();
        if (const auto conflictIt =
                overrideConflicts.find(hoveredConflictWidgetId);
            conflictIt != overrideConflicts.end()) {
          for (const auto &targetWidgetId :
               conflictIt->second.targetWidgetIds) {
            if (const auto rectIt = widgetRects.find(targetWidgetId);
                rectIt != widgetRects.end()) {
              drawList->AddRect(rectIt->second.Min, rectIt->second.Max,
                                IM_COL32(255, 204, 96, 255), 8.0f, 0, 3.0f);
            }
          }
        }
      }

      ImGui::EndTable();

      if (acceptedRowReorderPayload && acceptedRowReorderIndex >= 0) {
        ApplyRowReorder(*acceptedRowReorderPayload, acceptedRowReorderIndex,
                        acceptedRowInsertAfter);
      }
    }
    ImGui::EndChild();
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::BeginChild(
      "##override-delete-target", ImVec2(0.0f, 56.0f), ImGuiChildFlags_Borders,
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  const auto deletePos = ImGui::GetWindowPos();
  const auto deleteSize = ImGui::GetWindowSize();
  ImGui::TextWrapped("Drag an override or row here to delete it.");
  const ImRect deleteRect(deletePos, ImVec2(deletePos.x + deleteSize.x,
                                            deletePos.y + deleteSize.y));
  if (ImGui::BeginDragDropTargetCustom(
          deleteRect, ImGui::GetID("##override-delete-drop"))) {
    AcceptOverrideDeletePayload();
    ImGui::EndDragDropTarget();
  }
  ImGui::EndChild();
}
} // namespace sosng
