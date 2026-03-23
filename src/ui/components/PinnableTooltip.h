#pragma once

#include "imgui.h"

#include <functional>
#include <string_view>

namespace sosr::ui::components {
struct HoveredTooltipOptions {
  bool useCustomPlacement{false};
  ImVec2 pos{};
  ImVec2 pivot{0.0f, 0.0f};
};

void BeginPinnableTooltipFrame();
void EndPinnableTooltipFrame();
[[nodiscard]] bool HasPinnedTooltips();
void ClearPinnedTooltips();
[[nodiscard]] bool ShouldDrawPinnableTooltip(std::string_view a_id,
                                             bool a_hoveredSource);
void DrawPinnableTooltip(std::string_view a_id, bool a_hoveredSource,
                         const std::function<void()> &a_drawBody,
                         const HoveredTooltipOptions &a_hoveredOptions = {});
} // namespace sosr::ui::components
