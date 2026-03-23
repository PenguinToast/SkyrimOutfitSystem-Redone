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
constexpr float kWorkbenchRowGapY = 20.0f;
constexpr float kWorkbenchOverrideGapY = 5.0f;

struct ActiveWorkbenchVisual {
  std::string widgetId;
  std::string primaryName;
  std::string secondaryName;
  bool isOverride{false};
  std::string slotText;
  std::uint64_t slotMask{0};
  int rowIndex{-1};
};

struct ConflictEntry {
  std::string primaryName;
  std::string secondaryName;
  bool isOverride{false};
  bool isHideConflict{false};
  std::string targetLabel;
};

struct RowConflictInfo {
  std::vector<std::string> targetWidgetIds;
  std::vector<ConflictEntry> targetDescriptions;
};

struct OverrideConflictInfo {
  std::vector<std::string> targetWidgetIds;
  std::vector<ConflictEntry> targetDescriptions;
};

struct ColoredTextRun {
  std::string_view text;
  ImU32 color{0};
};

void DrawWrappedColoredTextRuns(
    const std::initializer_list<ColoredTextRun> &a_runs) {
  const auto wrapWidth = ImGui::GetContentRegionAvail().x;
  const auto lineHeight = ImGui::GetTextLineHeight();
  const auto origin = ImGui::GetCursorScreenPos();
  auto *drawList = ImGui::GetWindowDrawList();

  float x = 0.0f;
  float y = 0.0f;

  const auto advanceLine = [&]() {
    x = 0.0f;
    y += lineHeight;
  };

  for (const auto &run : a_runs) {
    std::size_t index = 0;
    while (index < run.text.size()) {
      const char current = run.text[index];
      if (current == '\n') {
        advanceLine();
        ++index;
        continue;
      }

      const bool isSpace = current == ' ' || current == '\t';
      std::size_t end = index;
      while (end < run.text.size()) {
        const char ch = run.text[end];
        if (ch == '\n') {
          break;
        }
        const bool sameClass = ((ch == ' ' || ch == '\t') == isSpace);
        if (!sameClass) {
          break;
        }
        ++end;
      }

      const auto token = run.text.substr(index, end - index);
      const auto tokenSize = ImGui::CalcTextSize(token.data(),
                                                 token.data() + token.size());

      if (isSpace) {
        if (x > 0.0f) {
          if (x + tokenSize.x > wrapWidth) {
            advanceLine();
          } else {
            x += tokenSize.x;
          }
        }
      } else {
        if (x > 0.0f && x + tokenSize.x > wrapWidth) {
          advanceLine();
        }

        drawList->AddText(ImVec2(origin.x + x, origin.y + y), run.color,
                          token.data(), token.data() + token.size());
        x += tokenSize.x;
      }

      index = end;
    }
  }

  ImGui::Dummy(ImVec2(wrapWidth, y + lineHeight));
}

bool CanHideEquippedVisual(const sosr::workbench::VariantWorkbenchRow &a_row) {
  return a_row.hideEquipped && a_row.isEquipped;
}

bool HasVariantSelectionConflictSource(
    const sosr::workbench::VariantWorkbenchRow &a_row) {
  return a_row.IsVisualConflictSource() &&
         (a_row.hideEquipped || !a_row.overrides.empty());
}

std::string DescribeRowSelectionReason(
    const sosr::workbench::VariantWorkbenchRow &a_row) {
  if (a_row.hideEquipped) {
    return "hide equipped";
  }
  return a_row.IsSlotRow() ? "slot override" : "item override";
}

template <class TConflictInfo>
void AppendConflictTarget(TConflictInfo &a_info,
                          const std::string_view a_widgetId,
                          ConflictEntry a_desc) {
  if (std::find(a_info.targetWidgetIds.begin(), a_info.targetWidgetIds.end(),
                a_widgetId) != a_info.targetWidgetIds.end()) {
    return;
  }

  a_info.targetWidgetIds.emplace_back(a_widgetId);
  a_info.targetDescriptions.push_back(std::move(a_desc));
}

void DrawConflictEntry(const ConflictEntry &a_desc,
                       const sosr::ThemeConfig *a_theme) {
  ImGui::Bullet();
  ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
  ImGui::BeginGroup();
  DrawWrappedColoredTextRuns(
      {{a_desc.primaryName,
        a_theme->GetColorU32(a_desc.isHideConflict ? "WARN" : "TEXT")},
       {a_desc.isHideConflict
            ? " hide equipped on "
            : (a_desc.isOverride ? " override on " : " "),
        a_theme->GetColorU32("TEXT_DISABLED")},
       {a_desc.secondaryName,
        a_theme->GetColorU32(a_desc.isHideConflict ? "TEXT" : "PRIMARY")},
       {a_desc.targetLabel.empty() ? "" : "  [", a_theme->GetColorU32("TEXT")},
       {a_desc.targetLabel, a_theme->GetColorU32("TEXT_HEADER", 0.92f)},
       {a_desc.targetLabel.empty() ? "" : "]", a_theme->GetColorU32("TEXT")}});

  ImGui::EndGroup();
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

bool Menu::ApplyWorkbenchRowDrop(const DraggedEquipmentPayload &a_dragPayload,
                                 const int a_targetRowIndex,
                                 const bool a_insertAfter) {
  if (a_targetRowIndex < 0) {
    if (a_dragPayload.sourceKind ==
        static_cast<std::uint32_t>(DragSourceKind::Catalog)) {
      return workbench_.AddCatalogSelectionAsRows(
          std::vector<RE::FormID>{a_dragPayload.formID});
    }
    if (a_dragPayload.sourceKind ==
        static_cast<std::uint32_t>(DragSourceKind::SlotCatalog)) {
      return workbench_.AddSlotRow(a_dragPayload.slotMask);
    }
    return false;
  }

  ApplyRowReorder(a_dragPayload, a_targetRowIndex, a_insertAfter);
  return true;
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
  } else if (a_dragPayload.sourceKind ==
             static_cast<std::uint32_t>(DragSourceKind::SlotCatalog)) {
    workbench_.InsertSlotRow(a_dragPayload.slotMask, a_targetRowIndex,
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
    ImGui::TextWrapped("No workbench rows yet. Drag equipment or slot entries "
                       "to the Equipped column to create one.");

    if (ImGui::BeginTable("##variant-workbench-empty", 3,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_Resizable,
                          ImVec2(0.0f, 180.0f))) {
      ImGui::TableSetupColumn("Equipped", ImGuiTableColumnFlags_WidthStretch,
                              0.80f);
      ImGui::TableSetupColumn("Overrides", ImGuiTableColumnFlags_WidthStretch,
                              1.05f);
      ImGui::TableSetupColumn("Hide",
                              ImGuiTableColumnFlags_WidthFixed |
                                  ImGuiTableColumnFlags_NoResize,
                              72.0f);
      ImGui::TableHeadersRow();
      ImGui::TableNextRow(ImGuiTableRowFlags_None, 116.0f);

      ImGui::TableSetColumnIndex(0);
      if (const auto *table = ImGui::GetCurrentTable(); table != nullptr) {
        const ImRect leftCellRect = ImGui::TableGetCellBgRect(table, 0);
        ImGui::SetCursorScreenPos(
            ImVec2(leftCellRect.Min.x + ImGui::GetStyle().CellPadding.x,
                   leftCellRect.Min.y + ImGui::GetStyle().CellPadding.y));
        ImGui::PushTextWrapPos(leftCellRect.Max.x -
                               ImGui::GetStyle().CellPadding.x);
        ImGui::TextDisabled("Drop equipment or equipment slots here.");
        ImGui::PopTextWrapPos();

        if (ImGui::BeginDragDropTargetCustom(
                leftCellRect, ImGui::GetID("##empty-workbench-row-target"))) {
          if (const auto *payload =
                  ImGui::AcceptDragDropPayload(kVariantItemPayloadType);
              payload && payload->Data != nullptr &&
              payload->DataSize == sizeof(DraggedEquipmentPayload)) {
            DraggedEquipmentPayload dragPayload{};
            std::memcpy(&dragPayload, payload->Data, sizeof(dragPayload));
            ApplyWorkbenchRowDrop(dragPayload);
          }
          ImGui::EndDragDropTarget();
        }
      }

      ImGui::TableSetColumnIndex(1);
      ImGui::TextDisabled("Add a row first, then drop equipment overrides here.");

      ImGui::TableSetColumnIndex(2);
      ImGui::TextDisabled("-");

      ImGui::EndTable();
    }
    return;
  }

  std::unordered_map<std::string, RowConflictInfo> rowConflicts;
  rowConflicts.reserve(rows.size());
  for (int rowIndex = 0; rowIndex < static_cast<int>(rows.size()); ++rowIndex) {
    const auto &row = rows[static_cast<std::size_t>(rowIndex)];
    if (!HasVariantSelectionConflictSource(row)) {
      continue;
    }

    RowConflictInfo info{};
    for (int otherRowIndex = 0; otherRowIndex < static_cast<int>(rows.size());
         ++otherRowIndex) {
      if (otherRowIndex == rowIndex) {
        continue;
      }

      const auto &otherRow = rows[static_cast<std::size_t>(otherRowIndex)];
      if (!HasVariantSelectionConflictSource(otherRow) ||
          (row.equipped.slotMask & otherRow.equipped.slotMask) == 0) {
        continue;
      }

      AppendConflictTarget(
          info, otherRow.key,
          {.primaryName = otherRow.equipped.name,
           .secondaryName = DescribeRowSelectionReason(otherRow),
           .targetLabel = otherRow.equipped.slotText});
    }

    if (!info.targetWidgetIds.empty()) {
      rowConflicts.emplace(row.key, std::move(info));
    }
  }

  std::vector<ActiveWorkbenchVisual> activeVisuals;
  activeVisuals.reserve(rows.size() * 2);
  for (int rowIndex = 0; rowIndex < static_cast<int>(rows.size()); ++rowIndex) {
    const auto &row = rows[static_cast<std::size_t>(rowIndex)];
    if (!row.IsVisualConflictSource()) {
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
             .primaryName = item.name,
             .secondaryName = row.equipped.name,
             .isOverride = true,
             .slotText = item.slotText,
             .slotMask = row.GetOverrideVisualSlotMask(item),
             .rowIndex = rowIndex});
      }
    } else if (!row.IsSlotRow() && !row.hideEquipped && row.isEquipped) {
      activeVisuals.push_back({.widgetId = row.key,
                               .primaryName = row.equipped.name,
                               .slotText = row.equipped.slotText,
                               .slotMask = row.equipped.slotMask,
                               .rowIndex = rowIndex});
    }
  }

  std::unordered_map<std::string, OverrideConflictInfo> overrideConflicts;
  overrideConflicts.reserve(rows.size() * 2);
  for (int rowIndex = 0; rowIndex < static_cast<int>(rows.size()); ++rowIndex) {
    const auto &row = rows[static_cast<std::size_t>(rowIndex)];
    if (!row.IsVisualConflictSource()) {
      continue;
    }

    for (int overrideIndex = 0;
         overrideIndex < static_cast<int>(row.overrides.size());
         ++overrideIndex) {
      const auto &item = row.overrides[static_cast<std::size_t>(overrideIndex)];
      const auto affectedSlotMask = row.GetOverrideVisualSlotMask(item);
      OverrideConflictInfo info{};
      for (const auto &activeVisual : activeVisuals) {
        if (activeVisual.rowIndex == rowIndex ||
            (activeVisual.slotMask & affectedSlotMask) == 0) {
          continue;
        }

        AppendConflictTarget(
            info, activeVisual.widgetId,
            {.primaryName = activeVisual.primaryName,
             .secondaryName = activeVisual.secondaryName,
             .isOverride = activeVisual.isOverride,
             .targetLabel = activeVisual.slotText});
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
                       static_cast<std::uint32_t>(DragSourceKind::Catalog) ||
                   a_payload->sourceKind ==
                       static_cast<std::uint32_t>(DragSourceKind::SlotCatalog);
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
      std::vector<std::string> hoveredConflictWidgetIds;
      std::vector<float> rowTopY;
      std::vector<float> rowBottomY;
      rowTopY.reserve(rows.size());
      rowBottomY.reserve(rows.size());

      for (int rowIndex = 0; rowIndex < static_cast<int>(rows.size());
           ++rowIndex) {
        const auto overrideCount =
            rows[static_cast<std::size_t>(rowIndex)].overrides.size();
        const auto widgetHeight = 18.0f + (ImGui::GetTextLineHeight() * 2.0f);
        const auto dropZoneHeight =
            overrideCount > 0
                ? (static_cast<float>(overrideCount) * widgetHeight) +
                      ((overrideCount > 1)
                           ? static_cast<float>(overrideCount - 1) *
                                 kWorkbenchOverrideGapY
                           : 0.0f)
                : widgetHeight;
        const auto contentHeight = (std::max)(widgetHeight, dropZoneHeight);
        const auto rowHeight = contentHeight + kWorkbenchRowGapY;
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
        const auto *table = ImGui::GetCurrentTable();
        if (table) {
          const ImRect leftCellRect = ImGui::TableGetCellBgRect(table, 0);
          const auto leftCellContentHeight =
              (leftCellRect.Max.y - leftCellRect.Min.y) -
              (ImGui::GetStyle().CellPadding.y * 2.0f);
          const auto leftCellContentOffsetY =
              (std::max)(0.0f, (leftCellContentHeight - contentHeight) * 0.5f);
          ImGui::SetCursorScreenPos(
              ImVec2(leftCellRect.Min.x + ImGui::GetStyle().CellPadding.x,
                     leftCellRect.Min.y + ImGui::GetStyle().CellPadding.y +
                         leftCellContentOffsetY));
        }
        const auto equippedWidget = ui::components::DrawEquipmentWidget(
            rows[static_cast<std::size_t>(rowIndex)].key.c_str(),
            rows[static_cast<std::size_t>(rowIndex)].equipped,
            {.showDeleteButton =
                 !rows[static_cast<std::size_t>(rowIndex)].isEquipped,
             .conflict = rowConflicts.contains(
                 rows[static_cast<std::size_t>(rowIndex)].key),
             .drawTooltipExtras =
                 rowConflicts.contains(rows[static_cast<std::size_t>(rowIndex)].key)
                     ? std::function<void()>{[&, rowIndex]() {
                         const auto *theme = ThemeConfig::GetSingleton();
                         const auto &row =
                             rows[static_cast<std::size_t>(rowIndex)];
                         const auto &info = rowConflicts.at(row.key);
                         ImGui::PushStyleColor(
                             ImGuiCol_Separator,
                             theme->GetColorU32("WARN"));
                         ImGui::Separator();
                         ImGui::PopStyleColor();
                         ImGui::Spacing();
                         ImGui::TextColored(
                             theme->GetColor("WARN"),
                             "Warning: Variant selection conflict.");
                         ImGui::Spacing();
                         for (const auto &description : info.targetDescriptions) {
                           ImGui::Bullet();
                           ImGui::SameLine(0.0f,
                                           ImGui::GetStyle().ItemInnerSpacing.x);
                           ImGui::BeginGroup();
                           DrawWrappedColoredTextRuns(
                               {{description.primaryName,
                                 theme->GetColorU32("TEXT")},
                                {description.secondaryName.empty() ? "" : " (",
                                 theme->GetColorU32("TEXT_DISABLED")},
                                {description.secondaryName,
                                 theme->GetColorU32("TEXT_DISABLED")},
                                {description.secondaryName.empty() ? "" : ")",
                                 theme->GetColorU32("TEXT_DISABLED")},
                                {description.targetLabel.empty() ? "" : "  [",
                                 theme->GetColorU32("TEXT")},
                                {description.targetLabel,
                                 theme->GetColorU32("TEXT_HEADER", 0.92f)},
                                {description.targetLabel.empty() ? "" : "]",
                                 theme->GetColorU32("TEXT")}});
                           ImGui::EndGroup();
                         }
                       }}
                     : std::function<void()>{}});
        if (!equippedWidget.deleteHovered && ImGui::BeginDragDropSource()) {
          DraggedEquipmentPayload payload{};
          payload.sourceKind = static_cast<std::uint32_t>(DragSourceKind::Row);
          payload.rowIndex = rowIndex;
          payload.itemIndex = -1;
          payload.formID =
              rows[static_cast<std::size_t>(rowIndex)].equipped.formID;
          payload.slotMask =
              rows[static_cast<std::size_t>(rowIndex)].equipped.slotMask;
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
        if (rowConflicts.contains(rows[static_cast<std::size_t>(rowIndex)].key) &&
            equippedWidget.hovered) {
          hoveredConflictWidgetIds =
              rowConflicts.at(rows[static_cast<std::size_t>(rowIndex)].key)
                  .targetWidgetIds;
        }
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
                      static_cast<std::uint32_t>(DragSourceKind::Catalog) ||
                  dragPayload.sourceKind ==
                      static_cast<std::uint32_t>(DragSourceKind::SlotCatalog)) {
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
        const auto cellPadding = ImGui::GetStyle().CellPadding;
        const auto overrideDropMin = ImVec2(overrideCellRect.Min.x + cellPadding.x,
                                            overrideCellRect.Min.y + cellPadding.y);
        const auto overrideDropMaxX = overrideCellRect.Max.x - cellPadding.x;
        const auto overrideCellContentHeight =
            (overrideCellRect.Max.y - overrideCellRect.Min.y) -
            (cellPadding.y * 2.0f);
        const auto overrideCellContentOffsetY =
            (std::max)(0.0f, (overrideCellContentHeight - contentHeight) * 0.5f);
        ImGui::SetCursorScreenPos(
            ImVec2(overrideCellRect.Min.x + cellPadding.x,
                   overrideCellRect.Min.y + cellPadding.y +
                       overrideCellContentOffsetY));

        if (overrideCount == 0) {
          const auto wrappedWidth =
              (overrideCellRect.Max.x - cellPadding.x) -
              (overrideCellRect.Min.x + cellPadding.x);
          const auto textSize = ImGui::CalcTextSize(
              "Drop equipment overrides here.", nullptr, false, wrappedWidth);
          ImGui::SetCursorScreenPos(
              ImVec2(overrideCellRect.Min.x + cellPadding.x,
                     overrideCellRect.Min.y + cellPadding.y +
                         overrideCellContentOffsetY +
                         ((dropZoneHeight - textSize.y) * 0.5f)));
          ImGui::PushTextWrapPos(overrideCellRect.Max.x - cellPadding.x);
          ImGui::TextWrapped("Drop equipment overrides here.");
          ImGui::PopTextWrapPos();
        } else {
          const auto oldItemSpacing = ImGui::GetStyle().ItemSpacing;
          ImGui::PushStyleVar(
              ImGuiStyleVar_ItemSpacing,
              ImVec2(oldItemSpacing.x, kWorkbenchOverrideGapY));
          for (int overrideIndex = 0;
               overrideIndex < static_cast<int>(overrideCount);
               ++overrideIndex) {
            const auto widgetId = "override:" + std::to_string(rowIndex) + ":" +
                                  std::to_string(overrideIndex);
            const auto overrideWidget = ui::components::DrawEquipmentWidget(
                widgetId.c_str(),
                rows[static_cast<std::size_t>(rowIndex)]
                    .overrides[static_cast<std::size_t>(overrideIndex)],
                {.showDeleteButton = true,
                 .conflict = overrideConflicts.contains(widgetId),
                 .drawTooltipExtras =
                     overrideConflicts.contains(widgetId)
                         ? std::function<void()>{[&, widgetId]() {
                             const auto *theme = ThemeConfig::GetSingleton();
                             ImGui::PushStyleColor(
                                 ImGuiCol_Separator,
                                 theme->GetColorU32("WARN"));
                             ImGui::Separator();
                             ImGui::PopStyleColor();
                             ImGui::Spacing();
                             ImGui::TextColored(
                                 theme->GetColor("WARN"),
                                 "Warning: Conflicting visuals.");
                             ImGui::Spacing();
                             for (const auto &description :
                                  overrideConflicts.at(widgetId)
                                      .targetDescriptions) {
                               DrawConflictEntry(description, theme);
                             }
                           }}
                         : std::function<void()>{}});
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
            if (overrideConflicts.contains(widgetId) && overrideWidget.hovered) {
              hoveredConflictWidgetIds =
                  overrideConflicts.at(widgetId).targetWidgetIds;
            }
          }
          ImGui::PopStyleVar();
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
          rowBottomY.push_back(bottomRect.Max.y);
        } else {
          rowBottomY.push_back(ImGui::GetItemRectMax().y);
        }

        const ImRect overrideDropRect(
            overrideDropMin,
            ImVec2(overrideDropMaxX,
                   rowBottomY.back() - cellPadding.y));
        ImGui::TableSetColumnIndex(1);
        if (ImGui::BeginDragDropTargetCustom(
                overrideDropRect,
                ImGui::GetID(
                    ("##override-cell-target-" + std::to_string(rowIndex))
                        .c_str()))) {
          AcceptOverridePayload(rowIndex);
          ImGui::EndDragDropTarget();
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
        if (const auto *table = ImGui::GetCurrentTable(); table != nullptr) {
          drawList->PushClipRect(table->OuterRect.Min, table->OuterRect.Max,
                                 false);
          drawList->AddLine(
              ImVec2(insertionLineX1, insertionLineY),
              ImVec2(insertionLineX2, insertionLineY),
              ThemeConfig::GetSingleton()->GetColorU32("PRIMARY"), 2.0f);
          drawList->PopClipRect();
        } else {
          drawList->AddLine(
              ImVec2(insertionLineX1, insertionLineY),
              ImVec2(insertionLineX2, insertionLineY),
              ThemeConfig::GetSingleton()->GetColorU32("PRIMARY"), 2.0f);
        }
      }

      if (!hoveredConflictWidgetIds.empty()) {
        auto *drawList = ImGui::GetWindowDrawList();
        for (const auto &targetWidgetId : hoveredConflictWidgetIds) {
          if (const auto rectIt = widgetRects.find(targetWidgetId);
              rectIt != widgetRects.end()) {
            drawList->AddRect(
                rectIt->second.Min, rectIt->second.Max,
                ThemeConfig::GetSingleton()->GetColorU32("WARN"), 8.0f, 0,
                3.0f);
          }
        }
      }

      ImGui::EndTable();

      if (acceptedRowReorderPayload && acceptedRowReorderIndex >= 0) {
        ApplyWorkbenchRowDrop(*acceptedRowReorderPayload,
                              acceptedRowReorderIndex,
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
