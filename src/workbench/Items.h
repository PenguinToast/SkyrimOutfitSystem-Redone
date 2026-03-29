#pragma once

#include <RE/Skyrim.h>

#include <cstdint>
#include <string>

namespace sosr::workbench {
enum class EquipmentWidgetItemKind : std::uint8_t { Armor, Slot };

struct EquipmentWidgetItem {
  EquipmentWidgetItemKind kind{EquipmentWidgetItemKind::Armor};
  RE::FormID formID{0};
  std::string key;
  std::string name;
  std::string slotText;
  std::uint64_t slotMask{0};

  [[nodiscard]] bool IsSlot() const {
    return kind == EquipmentWidgetItemKind::Slot;
  }

  [[nodiscard]] bool HasForm() const { return formID != 0; }

  [[nodiscard]] bool SupportsInfoTooltip() const {
    return !IsSlot() && HasForm();
  }
};
} // namespace sosr::workbench
