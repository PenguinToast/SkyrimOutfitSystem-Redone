#include "ui/InputSinkBridge.h"

#include "ui/Menu.h"

namespace sosr::ui {
InputSinkState GetInputSinkState() {
  const auto *menu = Menu::GetSingleton();
  return InputSinkState{
      .enabled = menu->IsEnabled(),
      .wantsTextInput = menu->WantsTextInput(),
      .capturingToggleKey = menu->IsCapturingToggleKey(),
      .toggleKey = menu->GetToggleKey(),
      .toggleModifier = menu->GetToggleModifier()};
}

void HandleToggleKeyCapture(const std::uint32_t a_scanCode,
                            const std::uint32_t a_modifierScanCode) {
  Menu::GetSingleton()->HandleToggleKeyCapture(a_scanCode, a_modifierScanCode);
}

void ToggleInputSinkVisibility() { Menu::GetSingleton()->Toggle(); }
} // namespace sosr::ui
