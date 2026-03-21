#include "ui/components/EquipmentWidget.h"

#include "ArmorUtils.h"
#include "imgui_internal.h"
#include "ui/ThemeConfig.h"
#include "ui/components/PinnableTooltip.h"

#include <algorithm>

namespace {
constexpr char kIconEditorId[] = "\xee\x84\x8b";   // ICON_LC_LIST
constexpr char kIconPlugin[] = "\xee\x84\xac";     // ICON_LC_PACKAGE
constexpr char kIconFormId[] = "\xee\x83\xb2";     // ICON_LC_HASH
constexpr char kIconIdentifier[] = "\xee\x84\x87"; // ICON_LC_LINK
constexpr char kIconSlot[] = "\xee\x87\x89";       // ICON_LC_SHIRT
constexpr char kIconTrash[] = "\xee\x86\x8c";      // ICON_LC_TRASH

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

float ComputeEquipmentTooltipWidth(
    const std::string &a_displayName, const std::string &a_editorID,
    const std::string &a_plugin, const std::string &a_formID,
    const std::string &a_identifier,
    const std::vector<std::string> &a_slotLabels) {
  float widestValueWidth = ImGui::CalcTextSize(a_displayName.c_str()).x;
  for (const auto *value : {a_editorID.c_str(), a_plugin.c_str(),
                            a_formID.c_str(), a_identifier.c_str()}) {
    widestValueWidth =
        (std::max)(widestValueWidth, ImGui::CalcTextSize(value).x);
  }
  for (const auto &slotLabel : a_slotLabels) {
    widestValueWidth =
        (std::max)(widestValueWidth, ImGui::CalcTextSize(slotLabel.c_str()).x);
  }

  return (std::max)(330.0f, widestValueWidth + 190.0f);
}

void DrawEquipmentInfoTooltipBody(
    const sosr::workbench::EquipmentWidgetItem &a_item,
    const std::string &a_displayName, const std::string &a_editorID,
    const std::string &a_plugin, const std::string &a_formID,
    const std::string &a_identifier,
    const std::vector<std::string> &a_slotLabels) {
  const auto *form = RE::TESForm::LookupByID(a_item.formID);
  if (!form) {
    return;
  }

  const auto *theme = sosr::ThemeConfig::GetSingleton();
  const auto headerMin = ImGui::GetCursorScreenPos();
  const auto headerWidth = ImGui::GetContentRegionAvail().x;
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
      ImGui::CalcTextSize(a_displayName.c_str(), nullptr, false, headerWidth);
  drawList->AddText(
      ImGui::GetFont(), titleFontSize,
      ImVec2(headerMin.x + (headerWidth - titleSize.x) * 0.5f,
             headerMin.y + (headerHeight - titleFontSize) * 0.5f - 1.0f),
      theme->GetColorU32("TEXT"), a_displayName.c_str());
  ImGui::Dummy(ImVec2(headerWidth, headerHeight));

  ImGui::Spacing();
  ImGui::PushStyleColor(ImGuiCol_Separator, theme->GetColorU32("PRIMARY"));
  ImGui::Separator();
  ImGui::PopStyleColor();
  ImGui::Spacing();

  if (ImGui::BeginTable("##equipment-info-tooltip", 2,
                        ImGuiTableFlags_NoSavedSettings |
                            ImGuiTableFlags_SizingFixedFit)) {
    ImGui::TableSetupColumn("##label", ImGuiTableColumnFlags_WidthFixed,
                            122.0f);
    ImGui::TableSetupColumn("##value", ImGuiTableColumnFlags_WidthStretch);

    DrawTooltipInfoRow(kIconEditorId, "Editor ID", a_editorID);
    DrawTooltipInfoRow(kIconPlugin, "Plugin", a_plugin);
    DrawTooltipInfoRow(kIconFormId, "Form ID", a_formID);
    DrawTooltipInfoRow(kIconIdentifier, "Identifier", a_identifier);
    for (std::size_t index = 0; index < a_slotLabels.size(); ++index) {
      DrawTooltipInfoRow(index == 0 ? kIconSlot : "", index == 0 ? "Slots" : "",
                         a_slotLabels[index]);
    }

    ImGui::EndTable();
  }
}
} // namespace

namespace sosr::ui::components {
bool BuildEquipmentTooltipItem(const RE::FormID a_formID, const char *a_key,
                               workbench::EquipmentWidgetItem &a_item) {
  const auto *form = RE::TESForm::LookupByID(a_formID);
  if (!form) {
    return false;
  }

  a_item.formID = a_formID;
  a_item.key = a_key ? a_key : "";
  a_item.name = sosr::armor::GetDisplayName(form);
  if (const auto *armor = form->As<RE::TESObjectARMO>()) {
    a_item.slotMask = armor->GetSlotMask().underlying();
    a_item.slotText = sosr::armor::JoinStrings(
        sosr::armor::GetArmorSlotLabels(a_item.slotMask));
  } else {
    a_item.slotMask = 0;
    a_item.slotText.clear();
  }

  return true;
}

void DrawEquipmentInfoTooltip(const std::string_view a_tooltipId,
                              const bool a_hoveredSource,
                              const workbench::EquipmentWidgetItem &a_item) {
  if (!ShouldDrawPinnableTooltip(a_tooltipId, a_hoveredSource)) {
    return;
  }

  const auto *form = RE::TESForm::LookupByID(a_item.formID);
  const auto displayName =
      form ? sosr::armor::GetDisplayName(form) : a_item.name;
  const auto editorID = form ? sosr::armor::GetEditorID(form) : std::string{};
  const auto plugin = form ? sosr::armor::GetPluginName(form) : std::string{};
  const auto formID = form ? sosr::armor::FormatFormID(form->GetFormID())
                           : sosr::armor::FormatFormID(a_item.formID);
  const auto identifier =
      form ? sosr::armor::GetFormIdentifier(form) : std::string{};
  const auto slotLabels = sosr::armor::GetArmorSlotLabels(a_item.slotMask);
  const auto tooltipContentWidth = ComputeEquipmentTooltipWidth(
      displayName, editorID, plugin, formID, identifier, slotLabels);
  ImGui::SetNextWindowSize(
      ImVec2(tooltipContentWidth + ImGui::GetStyle().WindowPadding.x * 2.0f,
             0.0f),
      ImGuiCond_Always);
  if (const auto mode = BeginPinnableTooltip(a_tooltipId, a_hoveredSource);
      mode != PinnableTooltipMode::None) {
    DrawEquipmentInfoTooltipBody(a_item, displayName, editorID, plugin, formID,
                                 identifier, slotLabels);
    EndPinnableTooltip(a_tooltipId, mode);
  }
}

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
    borderColor = a_options.conflict ? theme->GetColorU32("ERROR")
                                     : theme->GetColorU32("PRIMARY");
  } else if (result.hovered) {
    fillColor = a_options.conflict ? theme->GetColorU32("ERROR", 0.62f)
                                   : theme->GetColorU32("BG_LIGHT", 1.0f);
    borderColor = a_options.conflict ? theme->GetColorU32("ERROR")
                                     : theme->GetColorU32("PRIMARY", 0.70f);
  }

  drawList->AddRectFilled(rectMin, rectMax, fillColor, 8.0f);
  drawList->AddRect(rectMin, rectMax, borderColor, 8.0f);

  const auto namePos = ImVec2(rectMin.x + paddingX, rectMin.y + paddingY);
  const auto slotPos =
      ImVec2(rectMin.x + paddingX, rectMin.y + paddingY + lineHeight + 4.0f);
  const auto deletePaneWidth = a_options.showDeleteButton ? 34.0f : 0.0f;
  const auto clipMin = ImVec2(rectMin.x + paddingX, rectMin.y + paddingY);
  const auto clipMax =
      ImVec2(rectMax.x - paddingX - deletePaneWidth, rectMax.y - paddingY);
  const auto buttonMin = ImVec2(rectMax.x - deletePaneWidth, rectMin.y);
  const auto buttonMax = rectMax;
  result.deleteHovered = a_options.showDeleteButton &&
                         ImGui::IsMouseHoveringRect(buttonMin, buttonMax);

  drawList->PushClipRect(clipMin, clipMax, true);
  drawList->AddText(namePos, theme->GetColorU32("TEXT"), a_item.name.c_str());
  drawList->AddText(slotPos, theme->GetColorU32("TEXT_HEADER", 0.92f),
                    a_item.slotText.c_str());
  drawList->PopClipRect();

  if (a_options.showDeleteButton) {
    const auto deleteFill = result.deleteHovered
                                ? theme->GetColorU32("DECLINE", 0.95f)
                                : theme->GetColorU32("DECLINE", 0.78f);
    drawList->AddRectFilled(buttonMin, buttonMax, deleteFill, 8.0f,
                            ImDrawFlags_RoundCornersRight);
    drawList->AddLine(ImVec2(buttonMin.x, rectMin.y + 1.0f),
                      ImVec2(buttonMin.x, rectMax.y - 1.0f),
                      theme->GetColorU32("BORDER"), 1.0f);

    const char *deleteLabel = kIconTrash;
    const auto labelSize = ImGui::CalcTextSize(deleteLabel);
    drawList->AddText(
        ImVec2(buttonMin.x + ((deletePaneWidth - labelSize.x) * 0.5f),
               rectMin.y + ((frameHeight - labelSize.y) * 0.5f) - 1.0f),
        theme->GetColorU32("TEXT"), deleteLabel);

    if (result.deleteHovered && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
      result.deleteClicked = true;
    }
  }

  const auto tooltipId = "equipment:" + a_item.key;
  if (!result.deleteHovered && !ImGui::IsDragDropActive()) {
    DrawEquipmentInfoTooltip(tooltipId, result.hovered, a_item);
  }

  ImGui::PopID();
  return result;
}
} // namespace sosr::ui::components
