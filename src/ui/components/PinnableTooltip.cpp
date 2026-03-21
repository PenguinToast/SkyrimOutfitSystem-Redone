#include "ui/components/PinnableTooltip.h"

#include "imgui.h"

#include <string>

namespace {
struct PinnedTooltipState {
  std::string id;
  ImVec2 pos{};
  bool requestFocus{false};
  bool renderedThisFrame{false};
  bool hoveredThisFrame{false};
};

PinnedTooltipState &GetPinnedTooltipState() {
  static PinnedTooltipState state;
  return state;
}

[[nodiscard]] bool IsAltPressedThisFrame() {
  return ImGui::IsKeyPressed(ImGuiKey_LeftAlt, false) ||
         ImGui::IsKeyPressed(ImGuiKey_RightAlt, false);
}
} // namespace

namespace sosr::ui::components {
void BeginPinnableTooltipFrame() {
  auto &state = GetPinnedTooltipState();
  state.renderedThisFrame = false;
  state.hoveredThisFrame = false;
}

void EndPinnableTooltipFrame() {
  auto &state = GetPinnedTooltipState();
  if (state.id.empty()) {
    return;
  }

  if (!state.renderedThisFrame) {
    state = {};
    return;
  }

  if ((ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
       ImGui::IsMouseClicked(ImGuiMouseButton_Right) ||
       ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) &&
      !state.hoveredThisFrame) {
    state = {};
  }
}

bool ShouldDrawPinnableTooltip(const std::string_view a_id,
                               const bool a_hoveredSource) {
  const auto &state = GetPinnedTooltipState();
  return a_hoveredSource || state.id == a_id;
}

PinnableTooltipMode BeginPinnableTooltip(const std::string_view a_id,
                                         const bool a_hoveredSource) {
  auto &state = GetPinnedTooltipState();
  if (state.id == a_id) {
    state.renderedThisFrame = true;

    const auto windowName = "##PinnedTooltip_" + std::string(a_id);
    ImGui::SetNextWindowPos(state.pos, ImGuiCond_Always);
    ImGui::Begin(windowName.c_str(), nullptr,
                 ImGuiWindowFlags_Tooltip | ImGuiWindowFlags_NoDecoration |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_AlwaysAutoResize);
    if (state.requestFocus) {
      ImGui::SetWindowFocus();
      state.requestFocus = false;
    }
    return PinnableTooltipMode::Pinned;
  }

  if (!a_hoveredSource || !ImGui::BeginTooltip()) {
    return PinnableTooltipMode::None;
  }

  return PinnableTooltipMode::Hovered;
}

void EndPinnableTooltip(const std::string_view a_id,
                        const PinnableTooltipMode a_mode) {
  auto &state = GetPinnedTooltipState();
  if (a_mode == PinnableTooltipMode::None) {
    return;
  }

  if (a_mode == PinnableTooltipMode::Hovered) {
    if (IsAltPressedThisFrame()) {
      state.id = std::string(a_id);
      state.pos = ImGui::GetWindowPos();
      state.requestFocus = true;
      state.renderedThisFrame = true;
    }

    ImGui::EndTooltip();
    return;
  }

  state.renderedThisFrame = true;
  state.hoveredThisFrame =
      ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows |
                             ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
  ImGui::End();
}
} // namespace sosr::ui::components
