#include "Menu.h"

#include "ArmorUtils.h"
#include "ui/components/EquipmentWidget.h"

#include <algorithm>

namespace sosr {
bool Menu::DrawSlotTab() {
  struct SlotCatalogRow {
    workbench::EquipmentWidgetItem slotItem;
    std::vector<workbench::EquipmentWidgetItem> occupantItems;
  };

  std::vector<SlotCatalogRow> rows;
  rows.reserve(32);

  for (const auto slotMask : armor::GetAllArmorSlotMasks()) {
    SlotCatalogRow row{};
    if (!workbench_.BuildSlotItem(slotMask, row.slotItem)) {
      continue;
    }

    for (const auto &workbenchRow : workbench_.GetRows()) {
      if (!workbenchRow.isEquipped ||
          (workbenchRow.equipped.slotMask & slotMask) == 0 ||
          workbenchRow.equipped.formID == 0) {
        continue;
      }

      row.occupantItems.push_back(workbenchRow.equipped);
    }

    rows.push_back(std::move(row));
  }

  ImGui::Text("Results: %zu", rows.size());
  bool rowClicked = false;

  if (ImGui::BeginTable("##slot-table", 2,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
                        ImVec2(0.0f, 0.0f))) {
    ImGui::TableSetupColumn("Slot", ImGuiTableColumnFlags_WidthStretch, 0.68f);
    ImGui::TableSetupColumn("Occupied", ImGuiTableColumnFlags_WidthStretch,
                            1.00f);
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableHeadersRow();

    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(rows.size()));
    while (clipper.Step()) {
      for (int rowIndex = clipper.DisplayStart; rowIndex < clipper.DisplayEnd;
           ++rowIndex) {
        auto &row = rows[static_cast<std::size_t>(rowIndex)];
        const auto widgetHeight =
            18.0f + (ImGui::GetTextLineHeight() * 2.0f);
        const auto occupantHeight =
            row.occupantItems.empty()
                ? widgetHeight
                : (static_cast<float>(row.occupantItems.size()) * widgetHeight) +
                      ((row.occupantItems.size() > 1)
                           ? static_cast<float>(row.occupantItems.size() - 1) *
                                 5.0f
                           : 0.0f);
        const auto rowHeight = (std::max)(widgetHeight, occupantHeight);

        ImGui::TableNextRow(ImGuiTableRowFlags_None, rowHeight);
        ImGui::TableSetColumnIndex(0);
        const auto rowContentPos = ImGui::GetCursorScreenPos();
        ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, IM_COL32(0, 0, 0, 0));
        const bool selected = selectedCatalogKey_ == row.slotItem.key;
        const bool clicked = ImGui::Selectable(
            ("##slot-row-hit-" + std::to_string(rowIndex)).c_str(), selected,
            ImGuiSelectableFlags_SpanAllColumns |
                ImGuiSelectableFlags_AllowOverlap |
                ImGuiSelectableFlags_AllowDoubleClick,
            ImVec2(0.0f, rowHeight));
        const bool rowHovered = ImGui::IsItemHovered();
        ImGui::PopStyleColor(3);
        if (ImGui::BeginPopupContextItem()) {
          if (ImGui::MenuItem("Add to Workbench")) {
            workbench_.AddSlotRow(row.slotItem.slotMask);
          }
          ImGui::EndPopup();
        }
        ImGui::SetCursorScreenPos(rowContentPos);

        if (selected) {
          ImGui::TableSetBgColor(
              ImGuiTableBgTarget_RowBg0,
              ThemeConfig::GetSingleton()->GetColorU32("PRIMARY", 0.40f));
        } else if (rowHovered) {
          ImGui::TableSetBgColor(
              ImGuiTableBgTarget_RowBg0,
              ThemeConfig::GetSingleton()->GetColorU32("TABLE_HOVER", 0.12f));
        }

        DrawCatalogDragWidget(row.slotItem, DragSourceKind::SlotCatalog);

        if (clicked) {
          rowClicked = true;
          if (selectedCatalogKey_ == row.slotItem.key) {
            ClearCatalogSelection();
          } else {
            selectedCatalogKey_ = row.slotItem.key;
            workbench_.ClearPreview();
          }
        }

        if (rowHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
          rowClicked = true;
          workbench_.AddSlotRow(row.slotItem.slotMask);
        }

        ImGui::TableSetColumnIndex(1);
        if (!row.occupantItems.empty()) {
          const auto oldItemSpacing = ImGui::GetStyle().ItemSpacing;
          ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                              ImVec2(oldItemSpacing.x, 5.0f));
          for (std::size_t occupantIndex = 0;
               occupantIndex < row.occupantItems.size(); ++occupantIndex) {
            const auto widgetId =
                row.slotItem.key + ":occupant:" + std::to_string(occupantIndex);
            [[maybe_unused]] const auto occupantWidget =
                ui::components::DrawEquipmentWidget(
                    widgetId.c_str(), row.occupantItems[occupantIndex]);
          }
          ImGui::PopStyleVar();
        } else {
          ImGui::TextDisabled("Empty");
        }
      }
    }

    ImGui::EndTable();
  }

  return rowClicked;
}
} // namespace sosr
