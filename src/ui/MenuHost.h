#pragma once

namespace sosr {
class MenuHost : public RE::IMenu {
public:
  static constexpr std::string_view MENU_NAME = "SkyrimVanitySystemMenu";

  static void RegisterMenu();

  void PostDisplay() override;
  RE::UI_MESSAGE_RESULTS ProcessMessage(RE::UIMessage &a_message) override;

private:
  static RE::IMenu *Creator();
  static void ForceCursor();
};
} // namespace sosr
