#pragma once

#include <string_view>

namespace sosr::ui::components {
enum class PinnableTooltipMode : std::uint8_t {
  None,
  Hovered,
  HoveredOverlay,
  Pinned
};

void BeginPinnableTooltipFrame();
void EndPinnableTooltipFrame();
[[nodiscard]] bool HasPinnedTooltips();
void ClearPinnedTooltips();
[[nodiscard]] bool ShouldDrawPinnableTooltip(std::string_view a_id,
                                             bool a_hoveredSource);
[[nodiscard]] PinnableTooltipMode BeginPinnableTooltip(std::string_view a_id,
                                                       bool a_hoveredSource);
void EndPinnableTooltip(std::string_view a_id, PinnableTooltipMode a_mode);
} // namespace sosr::ui::components
