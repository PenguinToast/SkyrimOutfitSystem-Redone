#include "Hooks.h"

#include "InputManager.h"
#include "ui/Menu.h"
#include "ui/MenuHost.h"

namespace {
[[nodiscard]] bool
ShouldBlockKeyboardInputEvent(const RE::InputEvent *a_event) {
  if (a_event == nullptr) {
    return false;
  }

  const auto *menu = sosr::Menu::GetSingleton();
  if (!menu->IsEnabled() || !menu->WantsTextInput()) {
    return false;
  }

  switch (a_event->GetEventType()) {
  case RE::INPUT_EVENT_TYPE::kChar:
    return true;
  case RE::INPUT_EVENT_TYPE::kButton: {
    const auto *buttonEvent = static_cast<const RE::ButtonEvent *>(a_event);
    return buttonEvent->device == RE::INPUT_DEVICE::kKeyboard;
  }
  default:
    return false;
  }
}

void FilterBlockedInputEvents(RE::InputEvent **a_events) {
  if (a_events == nullptr) {
    return;
  }

  RE::InputEvent *previous = nullptr;
  RE::InputEvent *event = *a_events;
  while (event != nullptr) {
    RE::InputEvent *next = event->next;
    if (ShouldBlockKeyboardInputEvent(event)) {
      if (previous != nullptr) {
        previous->next = next;
      } else {
        *a_events = next;
      }
      event->next = nullptr;
    } else {
      previous = event;
    }
    event = next;
  }
}

static void
hk_PollInputDevices(RE::BSTEventSource<RE::InputEvent *> *a_dispatcher,
                    RE::InputEvent **a_events);
static inline REL::Relocation<decltype(hk_PollInputDevices)> g_inputHandler;
static inline REL::Relocation<uintptr_t> g_registerClass{
    REL::VariantID(75591, 77226, 0xDC4B90)};

void hk_PollInputDevices(RE::BSTEventSource<RE::InputEvent *> *a_dispatcher,
                         RE::InputEvent **a_events) {
  if (a_events) {
    sosr::InputManager::GetSingleton()->AddEventToQueue(a_events);
    FilterBlockedInputEvents(a_events);
  }

  g_inputHandler(a_dispatcher, a_events);
}

struct WndProcHook {
  static LRESULT thunk(HWND a_hwnd, UINT a_msg, WPARAM a_wParam,
                       LPARAM a_lParam) {
    switch (a_msg) {
    case WM_ACTIVATE: {
      const auto activationType = LOWORD(a_wParam);
      if (activationType != WA_INACTIVE) {
        sosr::InputManager::GetSingleton()->Flush();
        sosr::InputManager::GetSingleton()->OnFocusChange(true);
      }
      break;
    }
    case WM_SETFOCUS:
      sosr::InputManager::GetSingleton()->Flush();
      sosr::InputManager::GetSingleton()->OnFocusChange(true);
      break;
    case WM_KILLFOCUS:
      sosr::InputManager::GetSingleton()->OnFocusChange(false);
      break;
    default:
      break;
    }

    return func(a_hwnd, a_msg, a_wParam, a_lParam);
  }

  static inline REL::Relocation<decltype(thunk)> func;
};

struct RegisterClassAHook {
  static ATOM thunk(WNDCLASSA *a_wndClass) {
    WndProcHook::func = reinterpret_cast<uintptr_t>(a_wndClass->lpfnWndProc);
    a_wndClass->lpfnWndProc = &WndProcHook::thunk;
    return func(a_wndClass);
  }

  static inline REL::Relocation<decltype(thunk)> func;
};
} // namespace

namespace sosr::hooks {
struct D3DInitHook {
  static void thunk() {
    func();

    auto *renderer = RE::BSGraphics::Renderer::GetSingleton();
    auto *context = reinterpret_cast<ID3D11DeviceContext *>(
        renderer->GetRuntimeData().context);
    auto *swapChain = reinterpret_cast<IDXGISwapChain *>(
        renderer->GetRuntimeData().renderWindows->swapChain);
    auto *device =
        reinterpret_cast<ID3D11Device *>(renderer->GetRuntimeData().forwarder);

    Menu::GetSingleton()->Init(swapChain, device, context);
    MenuHost::RegisterMenu();
  }

  static inline REL::Relocation<decltype(thunk)> func;
};

struct PresentHook {
  static void thunk(std::uint32_t a_argument) {
    func(a_argument);
    InputManager::GetSingleton()->ProcessInputEvents();
  }

  static inline REL::Relocation<decltype(thunk)> func;
};

void Install() {
  auto &trampoline = SKSE::GetTrampoline();

  logger::info("Hooking BSInputDeviceManager::PollInputDevices");
  g_inputHandler =
      trampoline.write_call<5>(REL::RelocationID(67315, 68617).address() +
                                   REL::Relocate(0x7B, 0x7B, 0x81),
                               hk_PollInputDevices);

  logger::info("Hooking RegisterClassA");
  RegisterClassAHook::func =
      *reinterpret_cast<uintptr_t *>(trampoline.write_call<6>(
          g_registerClass.address() +
              REL::VariantOffset(0x8E, 0x15C, 0x99).offset(),
          RegisterClassAHook::thunk));

  logger::info("Hooking BSGraphics::Renderer::InitD3D");
  D3DInitHook::func = trampoline.write_call<5>(
      REL::RelocationID(75595, 77226).address() + REL::Relocate(0x50, 0x2BC),
      D3DInitHook::thunk);

  logger::info("Hooking DXGI present");
  PresentHook::func =
      trampoline.write_call<5>(REL::RelocationID(75461, 77246).address() +
                                   REL::VariantOffset(0x9, 0x9, 0x15).offset(),
                               PresentHook::thunk);
}
} // namespace sosr::hooks
