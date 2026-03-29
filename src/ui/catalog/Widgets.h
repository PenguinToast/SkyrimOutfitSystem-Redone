#pragma once

#include "EquipmentCatalog.h"

#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace sosr::ui::catalog {
[[nodiscard]] std::string TruncateTextToWidth(std::string_view a_text,
                                              float a_width);
void DrawOutfitTooltip(const OutfitEntry &a_outfit, bool a_hoveredSource);
void DrawKitTooltip(const KitEntry &a_kit, bool a_hoveredSource);
void DrawSimplePinnableTooltip(std::string_view a_id, bool a_hoveredSource,
                               const std::function<void()> &a_drawBody);
void DrawCatalogTabHelpTooltip(std::string_view a_id, bool a_hoveredSource,
                               std::initializer_list<const char *> a_lines);
[[nodiscard]] bool IsDelayedHover(float a_delaySeconds = 0.45f);
[[nodiscard]] std::optional<std::string> GetDavAvailabilityMessage();
void DrawDavAvailabilityBanner(std::string_view a_message);
} // namespace sosr::ui::catalog
