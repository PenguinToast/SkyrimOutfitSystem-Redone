#pragma once

#include "workbench/Items.h"

namespace sosr::workbench {
[[nodiscard]] bool BuildCatalogItem(RE::FormID a_formID,
                                    EquipmentWidgetItem &a_item);
[[nodiscard]] bool BuildSlotItem(std::uint64_t a_slotMask,
                                 EquipmentWidgetItem &a_item);
} // namespace sosr::workbench
