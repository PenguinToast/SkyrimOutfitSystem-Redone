#pragma once

namespace sosr {
class InputManager {
public:
  static InputManager *GetSingleton();

  void Init();
  void OnFocusChange(bool a_focus);
  void AddEventToQueue(RE::InputEvent **a_events);
  void ProcessInputEvents();
  void UpdateMousePosition() const;
  [[nodiscard]] bool IsBoundModifierDown() const;
  [[nodiscard]] std::uint32_t GetActiveModifierScanCode() const;

private:
  std::mutex inputLock_;
  std::vector<RE::InputEvent *> inputQueue_;
  bool shiftDown_{false};
  bool ctrlDown_{false};
  bool altDown_{false};
};
} // namespace sosr
