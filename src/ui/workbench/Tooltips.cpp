#include "ui/workbench/Tooltips.h"

#include "conditions/Validation.h"
#include "ui/components/PinnableTooltip.h"

namespace sosr::ui::workbench {
void DrawWrappedColoredTextRuns(
    const std::initializer_list<std::pair<std::string_view, ImU32>> a_runs) {
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

  for (const auto &[text, color] : a_runs) {
    std::size_t index = 0;
    while (index < text.size()) {
      const char current = text[index];
      if (current == '\n') {
        advanceLine();
        ++index;
        continue;
      }

      const bool isSpace = current == ' ' || current == '\t';
      std::size_t end = index;
      while (end < text.size()) {
        const char ch = text[end];
        if (ch == '\n') {
          break;
        }
        const bool sameClass = ((ch == ' ' || ch == '\t') == isSpace);
        if (!sameClass) {
          break;
        }
        ++end;
      }

      const auto token = text.substr(index, end - index);
      const auto tokenSize =
          ImGui::CalcTextSize(token.data(), token.data() + token.size());

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

        drawList->AddText(ImVec2(origin.x + x, origin.y + y), color,
                          token.data(), token.data() + token.size());
        x += tokenSize.x;
      }

      index = end;
    }
  }

  ImGui::Dummy(ImVec2(wrapWidth, y + lineHeight));
}

void DrawConflictEntry(
    const ui::workbench_conflicts::ConflictEntry &a_desc,
    const ThemeConfig *a_theme) {
  ImGui::Bullet();
  ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
  ImGui::BeginGroup();
  DrawWrappedColoredTextRuns(
      {{a_desc.primaryName,
        a_theme->GetColorU32(a_desc.isHideConflict ? "WARN" : "TEXT")},
       {a_desc.isHideConflict ? " hide equipped on "
                              : (a_desc.isOverride ? " override on " : " "),
        a_theme->GetColorU32("TEXT_DISABLED")},
       {a_desc.secondaryName,
        a_theme->GetColorU32(a_desc.isHideConflict ? "TEXT" : "PRIMARY")},
       {a_desc.targetLabel.empty() ? "" : "  [", a_theme->GetColorU32("TEXT")},
       {a_desc.targetLabel, a_theme->GetColorU32("TEXT_HEADER", 0.92f)},
       {a_desc.targetLabel.empty() ? "" : "]", a_theme->GetColorU32("TEXT")}});

  ImGui::EndGroup();
}

void DrawConflictTooltipSection(
    const char *a_title,
    const std::function<void(const ThemeConfig *)> &a_drawEntry) {
  const auto *theme = ThemeConfig::GetSingleton();
  ImGui::PushStyleColor(ImGuiCol_Separator, theme->GetColorU32("WARN"));
  ImGui::Separator();
  ImGui::PopStyleColor();
  ImGui::Spacing();
  ImGui::TextColored(theme->GetColor("WARN"), "%s", a_title);
  ImGui::Spacing();
  a_drawEntry(theme);
}

void DrawSimplePinnableTooltip(const std::string_view a_id,
                               const bool a_hoveredSource,
                               const std::function<void()> &a_drawBody) {
  ui::components::DrawPinnableTooltip(a_id, a_hoveredSource, a_drawBody);
}

RowConditionVisualState ResolveRowConditionVisualState(
    const ::sosr::workbench::VariantWorkbenchRow &a_row,
    const std::vector<ui::conditions::Definition> &a_conditions) {
  RowConditionVisualState state;
  if (!a_row.conditionId.has_value()) {
    state.name = "Disabled";
    state.description = "This row has no condition and will not apply.";
    state.disabled = true;
    return state;
  }

  if (const auto *condition =
          ::sosr::conditions::FindDefinitionById(a_conditions,
                                                 *a_row.conditionId);
      condition != nullptr) {
    state.color = ui::conditions::ToImGuiColor(condition->color);
    state.name = condition->name;
    state.description = condition->description;
    return state;
  }

  state.name = "Missing Condition";
  state.description =
      "The referenced condition no longer exists. This row will not apply.";
  state.disabled = true;
  state.missing = true;
  return state;
}
} // namespace sosr::ui::workbench
