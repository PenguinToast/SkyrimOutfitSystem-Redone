#include "InputManager.h"

#include "imgui.h"
#include "ui/Menu.h"

namespace sosr {
namespace {
auto MapScanCodeToImGuiKey(std::uint32_t a_scanCode) -> ImGuiKey {
  switch (a_scanCode) {
  case 0x01:
    return ImGuiKey_Escape;
  case 0x0E:
    return ImGuiKey_Backspace;
  case 0x0F:
    return ImGuiKey_Tab;
  case 0x1C:
    return ImGuiKey_Enter;
  case 0x39:
    return ImGuiKey_Space;
  case 0xC8:
    return ImGuiKey_UpArrow;
  case 0xD0:
    return ImGuiKey_DownArrow;
  case 0xCB:
    return ImGuiKey_LeftArrow;
  case 0xCD:
    return ImGuiKey_RightArrow;
  case 0xC9:
    return ImGuiKey_PageUp;
  case 0xD1:
    return ImGuiKey_PageDown;
  case 0xC7:
    return ImGuiKey_Home;
  case 0xCF:
    return ImGuiKey_End;
  case 0xD2:
    return ImGuiKey_Insert;
  case 0xD3:
    return ImGuiKey_Delete;
  case 0x2A:
    return ImGuiKey_LeftShift;
  case 0x36:
    return ImGuiKey_RightShift;
  case 0x1D:
    return ImGuiKey_LeftCtrl;
  case 0x9D:
    return ImGuiKey_RightCtrl;
  case 0x38:
    return ImGuiKey_LeftAlt;
  case 0xB8:
    return ImGuiKey_RightAlt;
  case 0x3B:
    return ImGuiKey_F1;
  case 0x3C:
    return ImGuiKey_F2;
  case 0x3D:
    return ImGuiKey_F3;
  case 0x3E:
    return ImGuiKey_F4;
  case 0x3F:
    return ImGuiKey_F5;
  case 0x40:
    return ImGuiKey_F6;
  case 0x41:
    return ImGuiKey_F7;
  case 0x42:
    return ImGuiKey_F8;
  case 0x43:
    return ImGuiKey_F9;
  case 0x44:
    return ImGuiKey_F10;
  case 0x57:
    return ImGuiKey_F11;
  case 0x58:
    return ImGuiKey_F12;
  default:
    return ImGuiKey_None;
  }
}
} // namespace

auto InputManager::GetSingleton() -> InputManager * {
  static InputManager singleton;
  return std::addressof(singleton);
}

void InputManager::Init() {}

void InputManager::OnFocusChange(bool a_focus) {
  if (ImGui::GetCurrentContext() == nullptr) {
    return;
  }

  auto &io = ImGui::GetIO();
  io.ClearInputKeys();
  io.ClearEventsQueue();
  io.AddFocusEvent(a_focus);

  shiftDown_ = false;
  ctrlDown_ = false;
  altDown_ = false;
}

void InputManager::Flush() {
  {
    std::scoped_lock lock(inputLock_);
    inputQueue_.clear();
  }

  if (auto *inputMgr = RE::BSInputDeviceManager::GetSingleton();
      inputMgr != nullptr) {
    if (auto *device = inputMgr->GetKeyboard(); device != nullptr) {
      device->ClearInputState();
    }
  }

  if (auto *eventQueue = RE::BSInputEventQueue::GetSingleton();
      eventQueue != nullptr) {
    eventQueue->ClearInputQueue();
  }

  shiftDown_ = false;
  ctrlDown_ = false;
  altDown_ = false;

  if (ImGui::GetCurrentContext() == nullptr) {
    return;
  }

  auto &io = ImGui::GetIO();
  io.ClearInputKeys();
  io.ClearEventsQueue();
}

void InputManager::AddEventToQueue(RE::InputEvent **a_events) {
  if (!a_events || !*a_events) {
    return;
  }

  std::scoped_lock lock(inputLock_);
  for (auto event = *a_events; event; event = event->next) {
    inputQueue_.push_back(event);
  }
}

void InputManager::ProcessInputEvents() {
  std::vector<RE::InputEvent *> queuedEvents;
  {
    std::scoped_lock lock(inputLock_);
    if (inputQueue_.empty()) {
      return;
    }

    queuedEvents.swap(inputQueue_);
  }

  auto *menu = Menu::GetSingleton();
  if (ImGui::GetCurrentContext() == nullptr) {
    return;
  }

  auto &io = ImGui::GetIO();

  for (const auto *event : queuedEvents) {
    switch (event->GetEventType()) {
    case RE::INPUT_EVENT_TYPE::kChar:
      break;
    case RE::INPUT_EVENT_TYPE::kButton: {
      const auto *buttonEvent = static_cast<const RE::ButtonEvent *>(event);
      const auto scanCode = buttonEvent->GetIDCode();
      const auto imguiKey = MapScanCodeToImGuiKey(scanCode);

      if (imguiKey == ImGuiKey_LeftShift || imguiKey == ImGuiKey_RightShift) {
        shiftDown_ = buttonEvent->IsPressed();
      } else if (imguiKey == ImGuiKey_LeftCtrl ||
                 imguiKey == ImGuiKey_RightCtrl) {
        ctrlDown_ = buttonEvent->IsPressed();
      } else if (imguiKey == ImGuiKey_LeftAlt ||
                 imguiKey == ImGuiKey_RightAlt) {
        altDown_ = buttonEvent->IsPressed();
      }

      switch (buttonEvent->device.get()) {
      case RE::INPUT_DEVICE::kMouse:
        break;
      case RE::INPUT_DEVICE::kKeyboard:
        if (menu->IsCapturingToggleKey() && buttonEvent->IsDown()) {
          if (keycode::IsKeyModifier(scanCode)) {
            break;
          }

          menu->HandleToggleKeyCapture(scanCode, GetActiveModifierScanCode());
          io.ClearInputKeys();
          break;
        }

        if (scanCode == menu->GetToggleKey() && IsBoundModifierDown() &&
            buttonEvent->IsDown()) {
          menu->Toggle();
          io.ClearInputKeys();
          break;
        }

        if (!menu->IsEnabled()) {
          break;
        }

        break;
      default:
        break;
      }
      break;
    }
    default:
      break;
    }
  }
}

bool InputManager::IsBoundModifierDown() const {
  switch (Menu::GetSingleton()->GetToggleModifier()) {
  case 0x00:
    return true;
  case 0x2A:
  case 0x36:
    return shiftDown_;
  case 0x1D:
  case 0x9D:
    return ctrlDown_;
  case 0x38:
  case 0xB8:
    return altDown_;
  default:
    return false;
  }
}

std::uint32_t InputManager::GetActiveModifierScanCode() const {
  if (shiftDown_) {
    return 0x2A;
  }
  if (ctrlDown_) {
    return 0x1D;
  }
  if (altDown_) {
    return 0x38;
  }
  return 0;
}

void InputManager::UpdateMousePosition() const {
  if (ImGui::GetCurrentContext() == nullptr) {
    return;
  }

  auto *ui = RE::UI::GetSingleton();
  if (ui == nullptr) {
    return;
  }

  auto &io = ImGui::GetIO();
  if (ui->IsMenuOpen(RE::CursorMenu::MENU_NAME)) {
    if (const auto *menuCursor = RE::MenuCursor::GetSingleton();
        menuCursor != nullptr) {
      io.AddMouseSourceEvent(ImGuiMouseSource_Mouse);
      io.AddMousePosEvent(menuCursor->cursorPosX, menuCursor->cursorPosY);
    }
    return;
  }

  POINT cursorPos{};
  if (GetCursorPos(&cursorPos) != FALSE) {
    io.AddMouseSourceEvent(ImGuiMouseSource_Mouse);
    io.AddMousePosEvent(static_cast<float>(cursorPos.x),
                        static_cast<float>(cursorPos.y));
  }
}
} // namespace sosr
