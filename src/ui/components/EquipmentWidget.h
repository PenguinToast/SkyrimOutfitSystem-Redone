#pragma once

#include "VariantWorkbench.h"

namespace sosr::ui::components {
struct EquipmentWidgetOptions {
  bool showDeleteButton{false};
  bool conflict{false};
};

struct EquipmentWidgetResult {
  bool hovered{false};
  bool active{false};
  bool deleteHovered{false};
  bool deleteClicked{false};
};

[[nodiscard]] EquipmentWidgetResult
DrawEquipmentWidget(const char *a_id,
                    const workbench::EquipmentWidgetItem &a_item,
                    const EquipmentWidgetOptions &a_options = {});
} // namespace sosr::ui::components
