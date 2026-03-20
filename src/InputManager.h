#pragma once

namespace sosng {
class InputManager {
public:
  static InputManager *GetSingleton();

  void Init();
  void OnFocusChange(bool a_focus);
  void AddEventToQueue(RE::InputEvent **a_events);
  void ProcessInputEvents();
  void UpdateMousePosition() const;

private:
  static constexpr std::uint32_t kToggleKey = 0x3D; // F3

  std::mutex inputLock_;
  std::vector<RE::InputEvent *> inputQueue_;
  bool shiftDown_{false};
  bool ctrlDown_{false};
  bool altDown_{false};
};
} // namespace sosng
