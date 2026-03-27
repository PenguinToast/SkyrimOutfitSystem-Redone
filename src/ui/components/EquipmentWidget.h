#pragma once

#include "VariantWorkbench.h"

#include <functional>

namespace sosr::ui::components {
enum class EquipmentWidgetConflictStyle { None, Warning, Error };

struct EquipmentWidgetOptions {
  bool showDeleteButton{false};
  EquipmentWidgetConflictStyle conflictStyle{
      EquipmentWidgetConflictStyle::None};
  std::function<void()> drawTooltipExtras{};
};

struct EquipmentWidgetResult {
  bool hovered{false};
  bool active{false};
  bool clicked{false};
  bool doubleClicked{false};
  bool deleteHovered{false};
  bool deleteClicked{false};
};

[[nodiscard]] bool
BuildEquipmentTooltipItem(RE::FormID a_formID, const char *a_key,
                          workbench::EquipmentWidgetItem &a_item);
void DrawEquipmentInfoTooltip(std::string_view a_tooltipId,
                              bool a_hoveredSource,
                              const workbench::EquipmentWidgetItem &a_item,
                              const std::function<void()> &a_drawExtras = {});
[[nodiscard]] EquipmentWidgetResult
DrawEquipmentWidget(const char *a_id,
                    const workbench::EquipmentWidgetItem &a_item,
                    const EquipmentWidgetOptions &a_options = {});
} // namespace sosr::ui::components
