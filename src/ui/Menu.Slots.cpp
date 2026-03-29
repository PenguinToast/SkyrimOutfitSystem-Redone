#include "Menu.h"

#include "ArmorUtils.h"
#include "ui/components/EquipmentWidget.h"

#include <algorithm>

namespace {
enum class SlotColumn : ImGuiID { Slot = 1, Occupied };

int CompareText(std::string_view a_left, std::string_view a_right) {
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
} // namespace

namespace sosr {
bool Menu::DrawSlotTab() {
  struct SlotCatalogRow {
    workbench::EquipmentWidgetItem slotItem;
    std::vector<workbench::EquipmentWidgetItem> occupantItems;
    std::string occupantSortText;
  };

  std::vector<SlotCatalogRow> rows;
  rows.reserve(32);

  for (const auto slotMask : armor::GetAllArmorSlotMasks()) {
    SlotCatalogRow row{};
    if (!workbench_.BuildSlotItem(slotMask, row.slotItem)) {
      continue;
    }

    for (const auto &workbenchRow : workbench_.GetRows()) {
      if (!workbenchRow.isEquipped || workbenchRow.equipped.formID == 0) {
        continue;
      }

      const auto *armor = RE::TESForm::LookupByID<RE::TESObjectARMO>(
          workbenchRow.equipped.formID);
      if (!armor || (armor::GetArmorAddonSlotMask(armor) & slotMask) == 0) {
        continue;
      }

      row.occupantItems.push_back(workbenchRow.equipped);
    }

    std::ranges::sort(row.occupantItems,
                      [](const auto &a_left, const auto &a_right) {
                        return CompareText(a_left.name, a_right.name) < 0;
                      });
    for (const auto &item : row.occupantItems) {
      if (!row.occupantSortText.empty()) {
        row.occupantSortText.append(", ");
      }
      row.occupantSortText.append(item.name);
    }
    if (row.occupantSortText.empty()) {
      row.occupantSortText = "Empty";
    }

    if (!showAllSlots_ && row.occupantItems.empty()) {
      continue;
    }

    rows.push_back(std::move(row));
  }

  ImGui::Text("Results: %zu", rows.size());
  bool rowClicked = false;

  if (ImGui::BeginTable("##slot-table", 2,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_Resizable |
                            ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY,
                        ImVec2(0.0f, 0.0f))) {
    ImGui::TableSetupColumn("Slot",
                            ImGuiTableColumnFlags_WidthStretch |
                                ImGuiTableColumnFlags_DefaultSort,
                            0.68f, static_cast<ImGuiID>(SlotColumn::Slot));
    ImGui::TableSetupColumn("Occupied", ImGuiTableColumnFlags_WidthStretch,
                            1.00f, static_cast<ImGuiID>(SlotColumn::Occupied));
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableHeadersRow();

    if (auto *sortSpecs = ImGui::TableGetSortSpecs();
        sortSpecs && sortSpecs->SpecsCount > 0) {
      const auto &spec = sortSpecs->Specs[0];
      const auto ascending = spec.SortDirection == ImGuiSortDirection_Ascending;
      std::ranges::sort(rows, [&](const auto &a_left, const auto &a_right) {
        int compare = 0;
        switch (static_cast<SlotColumn>(spec.ColumnUserID)) {
        case SlotColumn::Occupied:
          compare =
              CompareText(a_left.occupantSortText, a_right.occupantSortText);
          break;
        case SlotColumn::Slot:
        default:
          compare = CompareText(a_left.slotItem.name, a_right.slotItem.name);
          break;
        }

        if (compare == 0) {
          compare = CompareText(a_left.slotItem.name, a_right.slotItem.name);
        }

        return ascending ? compare < 0 : compare > 0;
      });
      sortSpecs->SpecsDirty = false;
    }

    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(rows.size()));
    while (clipper.Step()) {
      for (int rowIndex = clipper.DisplayStart; rowIndex < clipper.DisplayEnd;
           ++rowIndex) {
        auto &row = rows[static_cast<std::size_t>(rowIndex)];
        const auto widgetHeight = 18.0f + (ImGui::GetTextLineHeight() * 2.0f);
        const auto occupantHeight =
            row.occupantItems.empty()
                ? widgetHeight
                : (static_cast<float>(row.occupantItems.size()) *
                   widgetHeight) +
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

        const auto widgetResult =
            DrawCatalogDragWidget(row.slotItem, DragSourceKind::SlotCatalog);

        if (clicked || widgetResult.clicked) {
          rowClicked = true;
          if (selectedCatalogKey_ == row.slotItem.key) {
            ClearCatalogSelection();
          } else {
            selectedCatalogKey_ = row.slotItem.key;
            workbench_.ClearPreview();
          }
        }

        if ((rowHovered &&
             ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) ||
            widgetResult.doubleClicked) {
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
