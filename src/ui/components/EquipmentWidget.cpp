#include "ui/components/EquipmentWidget.h"

#include "ArmorUtils.h"
#include "ui/ThemeConfig.h"
#include "imgui_internal.h"

#include <algorithm>

namespace {
constexpr char kIconEditorId[] = "\xee\x84\x8b";   // ICON_LC_LIST
constexpr char kIconPlugin[] = "\xee\x84\xac";     // ICON_LC_PACKAGE
constexpr char kIconFormId[] = "\xee\x83\xb2";     // ICON_LC_HASH
constexpr char kIconIdentifier[] = "\xee\x84\x87"; // ICON_LC_LINK
constexpr char kIconSlot[] = "\xee\x87\x89";       // ICON_LC_SHIRT

void DrawTooltipInfoRow(const char *a_icon, const char *a_label,
                        const std::string &a_value) {
  if (a_value.empty()) {
    return;
  }

  const auto *theme = sosr::ThemeConfig::GetSingleton();
  ImGui::TableNextRow();

  ImGui::TableSetColumnIndex(0);
  if (a_icon && a_icon[0] != '\0') {
    ImGui::TextColored(theme->GetColor("PRIMARY"), "%s", a_icon);
    ImGui::SameLine(0.0f, 6.0f);
  }
  if (a_label && a_label[0] != '\0') {
    ImGui::TextDisabled("%s", a_label);
  }

  ImGui::TableSetColumnIndex(1);
  const auto availableWidth = ImGui::GetContentRegionAvail().x;
  const auto valueWidth = ImGui::CalcTextSize(a_value.c_str()).x;
  if (valueWidth < availableWidth) {
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + availableWidth - valueWidth);
    ImGui::TextUnformatted(a_value.c_str());
  } else {
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + availableWidth);
    ImGui::TextWrapped("%s", a_value.c_str());
    ImGui::PopTextWrapPos();
  }
}

void DrawEquipmentInfoTooltip(const sosr::workbench::EquipmentWidgetItem &a_item) {
  const auto *form = RE::TESForm::LookupByID(a_item.formID);
  if (!form) {
    return;
  }

  const auto *theme = sosr::ThemeConfig::GetSingleton();
  const auto displayName = sosr::armor::GetDisplayName(form);
  const auto editorID = sosr::armor::GetEditorID(form);
  const auto plugin = sosr::armor::GetPluginName(form);
  const auto formID = sosr::armor::FormatFormID(form->GetFormID());
  const auto identifier = sosr::armor::GetFormIdentifier(form);
  const auto slotLabels = sosr::armor::GetArmorSlotLabels(a_item.slotMask);
  float widestValueWidth = ImGui::CalcTextSize(displayName.c_str()).x;
  for (const auto *value : {editorID.c_str(), plugin.c_str(), formID.c_str(),
                            identifier.c_str()}) {
    widestValueWidth =
        (std::max)(widestValueWidth, ImGui::CalcTextSize(value).x);
  }
  for (const auto &slotLabel : slotLabels) {
    widestValueWidth =
        (std::max)(widestValueWidth, ImGui::CalcTextSize(slotLabel.c_str()).x);
  }

  ImGui::BeginTooltip();
  const auto tooltipCursor = ImGui::GetCursorPos();
  const auto tooltipWidth = (std::max)(330.0f, widestValueWidth + 190.0f);
  ImGui::Dummy(ImVec2(tooltipWidth, 0.0f));
  ImGui::SetCursorPos(tooltipCursor);

  const auto headerMin = ImGui::GetCursorScreenPos();
  const auto headerWidth =
      (std::max)(tooltipWidth, ImGui::GetContentRegionAvail().x);
  const auto headerHeight = ImGui::GetFontSize() * 2.4f;
  const auto headerMax =
      ImVec2(headerMin.x + headerWidth, headerMin.y + headerHeight);
  auto *drawList = ImGui::GetWindowDrawList();
  drawList->AddRectFilled(headerMin, headerMax, theme->GetColorU32("BG"), 8.0f);
  drawList->AddRect(headerMin, headerMax, theme->GetColorU32("BORDER"), 8.0f);
  drawList->AddRectFilledMultiColor(
      headerMin, headerMax, theme->GetColorU32("PRIMARY", 0.18f),
      theme->GetColorU32("PRIMARY", 0.18f), theme->GetColorU32("NONE"),
      theme->GetColorU32("NONE"));

  const auto titleFontSize = ImGui::GetFontSize() * 1.15f;
  const auto titleSize =
      ImGui::CalcTextSize(displayName.c_str(), nullptr, false, headerWidth);
  drawList->AddText(
      ImGui::GetFont(), titleFontSize,
      ImVec2(headerMin.x + (headerWidth - titleSize.x) * 0.5f,
             headerMin.y + (headerHeight - titleFontSize) * 0.5f - 1.0f),
      theme->GetColorU32("TEXT"), displayName.c_str());
  ImGui::Dummy(ImVec2(headerWidth, headerHeight));

  ImGui::Spacing();
  ImGui::PushStyleColor(ImGuiCol_Separator, theme->GetColorU32("PRIMARY"));
  ImGui::Separator();
  ImGui::PopStyleColor();
  ImGui::Spacing();

  if (ImGui::BeginTable("##equipment-info-tooltip", 2,
                        ImGuiTableFlags_NoSavedSettings |
                            ImGuiTableFlags_SizingFixedFit)) {
    ImGui::TableSetupColumn("##label", ImGuiTableColumnFlags_WidthFixed, 122.0f);
    ImGui::TableSetupColumn("##value", ImGuiTableColumnFlags_WidthStretch);

    DrawTooltipInfoRow(kIconEditorId, "Editor ID", editorID);
    DrawTooltipInfoRow(kIconPlugin, "Plugin", plugin);
    DrawTooltipInfoRow(kIconFormId, "Form ID", formID);
    DrawTooltipInfoRow(kIconIdentifier, "Identifier", identifier);
    for (std::size_t index = 0; index < slotLabels.size(); ++index) {
      DrawTooltipInfoRow(index == 0 ? kIconSlot : "",
                         index == 0 ? "Slots" : "", slotLabels[index]);
    }

    ImGui::EndTable();
  }

  ImGui::EndTooltip();
}
} // namespace

namespace sosr::ui::components {
EquipmentWidgetResult
DrawEquipmentWidget(const char *a_id,
                    const workbench::EquipmentWidgetItem &a_item,
                    const EquipmentWidgetOptions &a_options) {
  ImGui::PushID(a_id);

  constexpr float paddingX = 10.0f;
  constexpr float paddingY = 7.0f;
  const auto lineHeight = ImGui::GetTextLineHeight();
  const auto frameHeight = paddingY * 2.0f + lineHeight * 2.0f + 4.0f;
  const auto width = ImGui::GetContentRegionAvail().x;
  const auto size = ImVec2(width > 0.0f ? width : 1.0f, frameHeight);

  ImGui::InvisibleButton("##equipment-widget", size);

  EquipmentWidgetResult result{};
  result.hovered = ImGui::IsItemHovered();
  result.active = ImGui::IsItemActive();

  const auto rectMin = ImGui::GetItemRectMin();
  const auto rectMax = ImGui::GetItemRectMax();
  auto *drawList = ImGui::GetWindowDrawList();
  const auto *theme = ThemeConfig::GetSingleton();

  ImU32 fillColor = theme->GetColorU32("BG_LIGHT");
  ImU32 borderColor = theme->GetColorU32("BORDER");
  if (a_options.conflict) {
    fillColor = theme->GetColorU32("ERROR", 0.45f);
    borderColor = theme->GetColorU32("ERROR");
  }
  if (result.active) {
    fillColor = a_options.conflict ? theme->GetColorU32("ERROR", 0.75f)
                                   : theme->GetColorU32("PRIMARY", 0.80f);
    borderColor =
        a_options.conflict ? theme->GetColorU32("ERROR")
                           : theme->GetColorU32("PRIMARY");
  } else if (result.hovered) {
    fillColor = a_options.conflict ? theme->GetColorU32("ERROR", 0.62f)
                                   : theme->GetColorU32("BG_LIGHT", 1.0f);
    borderColor =
        a_options.conflict ? theme->GetColorU32("ERROR")
                           : theme->GetColorU32("PRIMARY", 0.70f);
  }

  drawList->AddRectFilled(rectMin, rectMax, fillColor, 8.0f);
  drawList->AddRect(rectMin, rectMax, borderColor, 8.0f);

  const auto namePos = ImVec2(rectMin.x + paddingX, rectMin.y + paddingY);
  const auto slotPos =
      ImVec2(rectMin.x + paddingX, rectMin.y + paddingY + lineHeight + 4.0f);
  const auto clipMin = ImVec2(rectMin.x + paddingX, rectMin.y + paddingY);
  const auto buttonWidth = a_options.showDeleteButton ? 24.0f : 0.0f;
  const auto clipMax =
      ImVec2(rectMax.x - paddingX - buttonWidth, rectMax.y - paddingY);
  const auto buttonSize = ImVec2(20.0f, 20.0f);
  const auto buttonMin =
      ImVec2(rectMax.x - paddingX - buttonSize.x, rectMin.y + paddingY);
  const auto buttonMax =
      ImVec2(buttonMin.x + buttonSize.x, buttonMin.y + buttonSize.y);
  result.deleteHovered = a_options.showDeleteButton &&
                         ImGui::IsMouseHoveringRect(buttonMin, buttonMax);

  drawList->PushClipRect(clipMin, clipMax, true);
  drawList->AddText(namePos, theme->GetColorU32("TEXT"), a_item.name.c_str());
  drawList->AddText(slotPos, theme->GetColorU32("TEXT_HEADER", 0.92f),
                    a_item.slotText.c_str());
  drawList->PopClipRect();

  if (a_options.showDeleteButton) {
    const auto deleteFill = result.deleteHovered ? theme->GetHover("DECLINE")
                                                 : theme->GetColor("DECLINE");
    drawList->AddRectFilled(buttonMin, buttonMax,
                            ImGui::ColorConvertFloat4ToU32(deleteFill), 4.0f);
    drawList->AddRect(buttonMin, buttonMax, theme->GetColorU32("DECLINE"),
                      4.0f);
    drawList->AddText(ImVec2(buttonMin.x + 6.0f, buttonMin.y + 2.0f),
                      theme->GetColorU32("TEXT"), "X");

    if (result.deleteHovered &&
        ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
      result.deleteClicked = true;
    }
  }

  if (result.hovered && !result.deleteHovered &&
      !ImGui::IsDragDropActive()) {
    DrawEquipmentInfoTooltip(a_item);
  }

  ImGui::PopID();
  return result;
}
} // namespace sosr::ui::components
