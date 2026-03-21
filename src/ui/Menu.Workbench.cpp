#include "Menu.h"

#include "imgui_internal.h"
#include "ui/components/EquipmentWidget.h"
#include "ui/components/PinnableTooltip.h"

#include <algorithm>
#include <cstring>
#include <functional>
#include <optional>
#include <unordered_map>

namespace {
constexpr char kVariantItemPayloadType[] = "SOSR_VARIANT_ITEM";

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
    const sosr::workbench::VariantWorkbenchRow &a_row,
    const sosr::workbench::EquipmentWidgetItem &a_item, bool a_isOverride) {
  if (a_isOverride) {
    return a_item.name + " override on " + a_row.equipped.name;
  }

  return a_row.equipped.name + " (equipped)";
}

void DrawSimplePinnableTooltip(const std::string_view a_id,
                               const bool a_hoveredSource,
                               const std::function<void()> &a_drawBody) {
  if (!sosr::ui::components::ShouldDrawPinnableTooltip(a_id, a_hoveredSource)) {
    return;
  }

  if (const auto mode =
          sosr::ui::components::BeginPinnableTooltip(a_id, a_hoveredSource);
      mode != sosr::ui::components::PinnableTooltipMode::None) {
    a_drawBody();
    sosr::ui::components::EndPinnableTooltip(a_id, mode);
  }
}
} // namespace

namespace sosr {
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

  const auto equippedKitFormIDs = workbench_.CollectEquippedArmorFormIDs();
  const auto overrideKitFormIDs =
      workbench_.CollectOverrideArmorFormIDsFromEquippedRows();

  if (ImGui::Button("Reset Equipped")) {
    workbench_.ClearPreview();
    if (workbench_.ResetEquippedRows()) {
      workbench_.SyncDynamicArmorVariantsExtended();
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Reset All")) {
    workbench_.ClearPreview();
    workbench_.ResetAllRows();
    workbench_.SyncRowsFromPlayer();
    workbench_.SyncDynamicArmorVariantsExtended();
  }
  ImGui::SameLine();
  const bool canCreateEquippedKit = !equippedKitFormIDs.empty();
  if (!canCreateEquippedKit) {
    ImGui::BeginDisabled();
  }
  if (ImGui::Button("Kit from Equipped")) {
    OpenCreateKitDialog(KitCreationSource::Equipped);
  }
  if (!canCreateEquippedKit) {
    ImGui::EndDisabled();
  }
  DrawSimplePinnableTooltip(
      "workbench:kit-from-equipped",
      ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort |
                           ImGuiHoveredFlags_AllowWhenDisabled),
      []() {
        ImGui::TextUnformatted(
            "Create a Modex kit from the player's currently equipped armor.");
      });
  ImGui::SameLine();
  const bool canCreateOverrideKit = !overrideKitFormIDs.empty();
  if (!canCreateOverrideKit) {
    ImGui::BeginDisabled();
  }
  if (ImGui::Button("Kit from Overrides")) {
    OpenCreateKitDialog(KitCreationSource::Overrides);
  }
  if (!canCreateOverrideKit) {
    ImGui::EndDisabled();
  }
  DrawSimplePinnableTooltip(
      "workbench:kit-from-overrides",
      ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort |
                           ImGuiHoveredFlags_AllowWhenDisabled),
      []() {
        ImGui::TextUnformatted("Create a Modex kit from overrides on currently "
                               "equipped armor only.");
      });
  ImGui::Spacing();

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
        const auto widgetHeight = 18.0f + (ImGui::GetTextLineHeight() * 2.0f);
        const auto overrideSpacingY = ImGui::GetStyle().ItemSpacing.y;
        const auto dropZoneHeight =
            overrideCount > 0
                ? (static_cast<float>(overrideCount) * widgetHeight) +
                      ((overrideCount > 1)
                           ? static_cast<float>(overrideCount - 1) *
                                 overrideSpacingY
                           : 0.0f)
                : widgetHeight;
        const auto rowHeight = (std::max)(widgetHeight, dropZoneHeight);
        ImGui::TableNextRow(ImGuiTableRowFlags_None, rowHeight);
        ImGui::TableSetBgColor(
            ImGuiTableBgTarget_RowBg0,
            ThemeConfig::GetSingleton()->GetColorU32(
                (rowIndex % 2) == 0 ? "TABLE_BG" : "TABLE_BG_ALT"));
        if (rows[static_cast<std::size_t>(rowIndex)].isEquipped) {
          ImGui::TableSetBgColor(
              ImGuiTableBgTarget_RowBg1,
              ThemeConfig::GetSingleton()->GetColorU32("SECONDARY", 0.16f));
        }

        ImGui::TableSetColumnIndex(0);
        const auto equippedWidget = ui::components::DrawEquipmentWidget(
            rows[static_cast<std::size_t>(rowIndex)].key.c_str(),
            rows[static_cast<std::size_t>(rowIndex)].equipped,
            {.showDeleteButton =
                 !rows[static_cast<std::size_t>(rowIndex)].isEquipped});
        if (!equippedWidget.deleteHovered && ImGui::BeginDragDropSource()) {
          DraggedEquipmentPayload payload{};
          payload.sourceKind = static_cast<std::uint32_t>(DragSourceKind::Row);
          payload.rowIndex = rowIndex;
          payload.itemIndex = -1;
          payload.formID =
              rows[static_cast<std::size_t>(rowIndex)].equipped.formID;
          ImGui::SetDragDropPayload(kVariantItemPayloadType, &payload,
                                    sizeof(payload));
          ImGui::TextUnformatted(
              rows[static_cast<std::size_t>(rowIndex)].equipped.name.c_str());
          ImGui::Text("%s", rows[static_cast<std::size_t>(rowIndex)]
                                .equipped.slotText.c_str());
          ImGui::EndDragDropSource();
        }
        if (equippedWidget.deleteClicked) {
          workbench_.DeleteRow(rowIndex);
          break;
        }
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
        const auto *currentTable = ImGui::GetCurrentTable();
        const auto overrideCellRect =
            currentTable ? ImGui::TableGetCellBgRect(currentTable, 1)
                         : ImRect(ImGui::GetCursorScreenPos(),
                                  ImGui::GetCursorScreenPos());
        const auto overrideDropRect =
            currentTable
                ? ImRect(overrideCellRect.Min,
                         ImVec2(overrideCellRect.Max.x,
                                overrideCellRect.Max.y +
                                    currentTable->RowCellPaddingY))
                : overrideCellRect;
        const auto cellPadding = ImGui::GetStyle().CellPadding;
        ImGui::SetCursorScreenPos(
            ImVec2(overrideCellRect.Min.x + cellPadding.x,
                   overrideCellRect.Min.y + cellPadding.y));

        if (overrideCount == 0) {
          ImGui::PushTextWrapPos(overrideCellRect.Max.x - cellPadding.x);
          ImGui::TextWrapped("Drop equipment overrides here.");
          ImGui::PopTextWrapPos();
        } else {
          for (int overrideIndex = 0;
               overrideIndex < static_cast<int>(overrideCount);
               ++overrideIndex) {
            if (overrideIndex > 0) {
              ImGui::Spacing();
            }

            const auto widgetId = "override:" + std::to_string(rowIndex) + ":" +
                                  std::to_string(overrideIndex);
            const auto overrideWidget = ui::components::DrawEquipmentWidget(
                widgetId.c_str(),
                rows[static_cast<std::size_t>(rowIndex)]
                    .overrides[static_cast<std::size_t>(overrideIndex)],
                {.showDeleteButton = true,
                 .conflict = overrideConflicts.contains(widgetId)});
            if (!overrideWidget.deleteHovered && ImGui::BeginDragDropSource()) {
              DraggedEquipmentPayload payload{};
              payload.sourceKind =
                  static_cast<std::uint32_t>(DragSourceKind::Override);
              payload.rowIndex = rowIndex;
              payload.itemIndex = overrideIndex;
              payload.formID =
                  rows[static_cast<std::size_t>(rowIndex)]
                      .overrides[static_cast<std::size_t>(overrideIndex)]
                      .formID;
              ImGui::SetDragDropPayload(kVariantItemPayloadType, &payload,
                                        sizeof(payload));
              ImGui::TextUnformatted(
                  rows[static_cast<std::size_t>(rowIndex)]
                      .overrides[static_cast<std::size_t>(overrideIndex)]
                      .name.c_str());
              ImGui::Text(
                  "%s", rows[static_cast<std::size_t>(rowIndex)]
                            .overrides[static_cast<std::size_t>(overrideIndex)]
                            .slotText.c_str());
              ImGui::EndDragDropSource();
            }
            if (overrideWidget.deleteClicked) {
              workbench_.DeleteOverride(rowIndex, overrideIndex);
              break;
            }
            widgetRects.insert_or_assign(
                widgetId,
                ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()));
            if (overrideConflicts.contains(widgetId) &&
                overrideWidget.hovered) {
              hoveredConflictWidgetId = widgetId;
              DrawSimplePinnableTooltip(
                  "workbench:conflict:" + widgetId, overrideWidget.hovered,
                  [&]() {
                    ImGui::TextWrapped("This override shares equipment slots "
                                       "with currently active visuals.");
                    ImGui::Separator();
                    for (const auto &description :
                         overrideConflicts.at(widgetId).targetDescriptions) {
                      ImGui::BulletText("%s", description.c_str());
                    }
                  });
            }
          }
        }

        if (ImGui::BeginDragDropTargetCustom(
                overrideDropRect,
                ImGui::GetID(
                    ("##override-cell-target-" + std::to_string(rowIndex))
                        .c_str()))) {
          AcceptOverridePayload(rowIndex);
          ImGui::EndDragDropTarget();
        }

        ImGui::TableSetColumnIndex(2);
        bool hideEquipped =
            rows[static_cast<std::size_t>(rowIndex)].hideEquipped;
        if (const auto *hideTable = ImGui::GetCurrentTable();
            hideTable != nullptr) {
          const auto hideCellRect = ImGui::TableGetCellBgRect(hideTable, 2);
          const auto checkboxSize =
              ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight());
          const auto checkboxPos = ImVec2(
              hideCellRect.Min.x +
                  ((hideCellRect.Max.x - hideCellRect.Min.x) - checkboxSize.x) *
                      0.5f,
              hideCellRect.Min.y +
                  ((hideCellRect.Max.y - hideCellRect.Min.y) - checkboxSize.y) *
                      0.5f);
          ImGui::SetCursorScreenPos(checkboxPos);
        }
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
                          ThemeConfig::GetSingleton()->GetColorU32("PRIMARY"),
                          2.0f);
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
              drawList->AddRect(
                  rectIt->second.Min, rectIt->second.Max,
                  ThemeConfig::GetSingleton()->GetColorU32("WARN"), 8.0f, 0,
                  3.0f);
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
} // namespace sosr
