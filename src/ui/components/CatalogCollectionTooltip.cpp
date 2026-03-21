#include "ui/components/CatalogCollectionTooltip.h"

#include "ui/ThemeConfig.h"
#include "imgui_internal.h"

#include <algorithm>

namespace {
void DrawTooltipInfoRow(const char *a_icon, const std::string &a_label,
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
  ImGui::TextDisabled("%s", a_label.c_str());

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
} // namespace

namespace sosr::ui::components {
void DrawCatalogCollectionTooltip(
    std::string_view a_title,
    const std::vector<CatalogTooltipMetaRow> &a_metaRows,
    const std::vector<CatalogTooltipItemRow> &a_items) {
  const auto *theme = sosr::ThemeConfig::GetSingleton();
  const auto &style = ImGui::GetStyle();
  float widestMetaValueWidth = ImGui::CalcTextSize(a_title.data()).x;
  float widestItemLabelWidth = 0.0f;
  float widestItemValueWidth = 0.0f;
  for (const auto &row : a_metaRows) {
    widestMetaValueWidth = (std::max)(
        widestMetaValueWidth, ImGui::CalcTextSize(row.value.c_str()).x);
  }
  for (const auto &item : a_items) {
    widestItemLabelWidth = (std::max)(
        widestItemLabelWidth, ImGui::CalcTextSize(item.name.c_str()).x);
    widestItemValueWidth = (std::max)(
        widestItemValueWidth, ImGui::CalcTextSize(item.slots.c_str()).x);
  }

  const auto metaSectionWidth = widestMetaValueWidth + 190.0f;
  const auto itemSectionWidth =
      widestItemLabelWidth + widestItemValueWidth +
      style.CellPadding.x * 4.0f + style.ItemSpacing.x + 24.0f;
  const auto tooltipContentWidth =
      (std::max)(360.0f, (std::max)(metaSectionWidth, itemSectionWidth));
  ImGui::SetNextWindowSize(
      ImVec2(tooltipContentWidth + style.WindowPadding.x * 2.0f, 0.0f),
      ImGuiCond_Always);
  ImGui::BeginTooltip();

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
      ImGui::CalcTextSize(a_title.data(), nullptr, false, headerWidth);
  drawList->AddText(
      ImGui::GetFont(), titleFontSize,
      ImVec2(headerMin.x + (headerWidth - titleSize.x) * 0.5f,
             headerMin.y + (headerHeight - titleFontSize) * 0.5f - 1.0f),
      theme->GetColorU32("TEXT"), a_title.data());
  ImGui::Dummy(ImVec2(headerWidth, headerHeight));

  ImGui::Spacing();
  ImGui::PushStyleColor(ImGuiCol_Separator, theme->GetColorU32("PRIMARY"));
  ImGui::Separator();
  ImGui::PopStyleColor();
  ImGui::Spacing();

  if (ImGui::BeginTable("##catalog-collection-meta", 2,
                        ImGuiTableFlags_NoSavedSettings |
                            ImGuiTableFlags_SizingFixedFit)) {
    ImGui::TableSetupColumn("##label", ImGuiTableColumnFlags_WidthFixed, 122.0f);
    ImGui::TableSetupColumn("##value", ImGuiTableColumnFlags_WidthStretch);
    for (const auto &row : a_metaRows) {
      DrawTooltipInfoRow(row.icon, row.label, row.value);
    }
    ImGui::EndTable();
  }

  if (!a_items.empty()) {
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator, theme->GetColorU32("PRIMARY"));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    constexpr auto sectionTitle = "Included Items";
    const auto sectionWidth = ImGui::CalcTextSize(sectionTitle).x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                         (ImGui::GetContentRegionAvail().x - sectionWidth) *
                             0.5f);
    ImGui::TextUnformatted(sectionTitle);
    ImGui::Spacing();

    if (ImGui::BeginTable("##catalog-collection-items", 2,
                          ImGuiTableFlags_NoSavedSettings |
                              ImGuiTableFlags_SizingFixedFit)) {
      ImGui::TableSetupColumn("##item-name", ImGuiTableColumnFlags_WidthFixed,
                              widestItemLabelWidth + 8.0f);
      ImGui::TableSetupColumn("##item-slots",
                              ImGuiTableColumnFlags_WidthStretch);

      for (const auto &item : a_items) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted(item.name.c_str());
        ImGui::TableSetColumnIndex(1);
        const auto availableWidth = ImGui::GetContentRegionAvail().x;
        const auto slotsWidth = ImGui::CalcTextSize(item.slots.c_str()).x;
        if (slotsWidth < availableWidth) {
          ImGui::SetCursorPosX(ImGui::GetCursorPosX() + availableWidth - slotsWidth);
        }
        ImGui::TextUnformatted(item.slots.c_str());
      }

      ImGui::EndTable();
    }
  }

  ImGui::EndTooltip();
}
} // namespace sosr::ui::components
