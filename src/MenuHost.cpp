#include "MenuHost.h"

#include "Menu.h"

namespace sosng {
void MenuHost::RegisterMenu() {
  static bool registered = false;
  if (registered) {
    return;
  }

  if (auto *ui = RE::UI::GetSingleton(); ui != nullptr) {
    ui->Register(MENU_NAME, Creator);
    registered = true;
  }
}

void MenuHost::PostDisplay() {
  Menu::GetSingleton()->Draw();
  ForceCursor();
}

RE::UI_MESSAGE_RESULTS MenuHost::ProcessMessage(RE::UIMessage &a_message) {
  switch (a_message.type.get()) {
  case RE::UI_MESSAGE_TYPE::kShow:
    Menu::GetSingleton()->OnMenuShow();
    return RE::UI_MESSAGE_RESULTS::kHandled;
  case RE::UI_MESSAGE_TYPE::kHide:
    Menu::GetSingleton()->OnMenuHide();
    return RE::UI_MESSAGE_RESULTS::kHandled;
  default:
    return IMenu::ProcessMessage(a_message);
  }
}

RE::IMenu *MenuHost::Creator() {
  using Flags = RE::UI_MENU_FLAGS;
  auto *menu = new MenuHost();
  menu->menuFlags.set(Flags::kUpdateUsesCursor, Flags::kUsesCursor);
  menu->menuFlags.set(Flags::kCustomRendering);
  menu->menuFlags.set(Flags::kUsesMenuContext);
  menu->depthPriority = 11;

  if (Menu::GetSingleton()->PauseGameWhenOpen()) {
    menu->menuFlags.set(Flags::kPausesGame);
  }

  menu->inputContext.set(Context::kMenuMode);
  return menu;
}

void MenuHost::ForceCursor() {
  auto *ui = RE::UI::GetSingleton();
  if (!ui || ui->IsMenuOpen(RE::CursorMenu::MENU_NAME)) {
    return;
  }

  SKSE::GetTaskInterface()->AddUITask([]() {
    if (auto *messagingQueue = RE::UIMessageQueue::GetSingleton();
        messagingQueue != nullptr) {
      messagingQueue->AddMessage(RE::CursorMenu::MENU_NAME,
                                 RE::UI_MESSAGE_TYPE::kShow, nullptr);
    }
  });
}
} // namespace sosng
