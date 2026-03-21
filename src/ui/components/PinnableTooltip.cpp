#include "ui/components/PinnableTooltip.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <algorithm>
#include <string>
#include <vector>

namespace {
struct PinnedTooltipState {
  std::string id;
  ImVec2 pos{};
  ImVec2 size{};
  bool hadVerticalScroll{false};
  bool requestFocus{false};
  bool renderedThisFrame{false};
  bool hoveredThisFrame{false};
};

struct PinnedTooltipManager {
  std::vector<PinnedTooltipState> stack;
  bool altWasDownLastFrame{false};
  bool altPressedThisFrame{false};
};

PinnedTooltipManager &GetPinnedTooltipManager() {
  static PinnedTooltipManager manager;
  return manager;
}

auto FindPinnedTooltip(std::vector<PinnedTooltipState> &a_stack,
                       const std::string_view a_id) {
  return std::find_if(
      a_stack.begin(), a_stack.end(),
      [&](const PinnedTooltipState &a_state) { return a_state.id == a_id; });
}

} // namespace

namespace sosr::ui::components {
void BeginPinnableTooltipFrame() {
  auto &manager = GetPinnedTooltipManager();
  const bool altDownThisFrame = ImGui::GetIO().KeyAlt;
  manager.altPressedThisFrame =
      altDownThisFrame && !manager.altWasDownLastFrame;
  manager.altWasDownLastFrame = altDownThisFrame;

  for (auto &state : manager.stack) {
    state.renderedThisFrame = false;
    state.hoveredThisFrame = false;
  }
}

void EndPinnableTooltipFrame() {
  auto &manager = GetPinnedTooltipManager();
  if (manager.stack.empty()) {
    return;
  }

  manager.stack.erase(std::remove_if(manager.stack.begin(), manager.stack.end(),
                                     [](const PinnedTooltipState &a_state) {
                                       return !a_state.renderedThisFrame;
                                     }),
                      manager.stack.end());
  if (manager.stack.empty()) {
    return;
  }

  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
      ImGui::IsMouseClicked(ImGuiMouseButton_Right) ||
      ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
    std::ptrdiff_t hoveredIndex = -1;
    for (std::ptrdiff_t index =
             static_cast<std::ptrdiff_t>(manager.stack.size()) - 1;
         index >= 0; --index) {
      if (manager.stack[static_cast<std::size_t>(index)].hoveredThisFrame) {
        hoveredIndex = index;
        break;
      }
    }

    if (hoveredIndex < 0) {
      manager.stack.clear();
    } else {
      manager.stack.resize(static_cast<std::size_t>(hoveredIndex) + 1);
    }
  }
}

bool ShouldDrawPinnableTooltip(const std::string_view a_id,
                               const bool a_hoveredSource) {
  auto &manager = GetPinnedTooltipManager();
  return a_hoveredSource ||
         FindPinnedTooltip(manager.stack, a_id) != manager.stack.end();
}

PinnableTooltipMode BeginPinnableTooltip(const std::string_view a_id,
                                         const bool a_hoveredSource) {
  auto &manager = GetPinnedTooltipManager();
  const auto pinnedIt = FindPinnedTooltip(manager.stack, a_id);
  if (pinnedIt != manager.stack.end()) {
    pinnedIt->renderedThisFrame = true;

    const auto windowName = "##PinnedTooltip_" + std::string(a_id);
    ImGui::SetNextWindowPos(pinnedIt->pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(pinnedIt->size, ImGuiCond_Always);
    auto flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;
    if (pinnedIt->hadVerticalScroll) {
      flags |= ImGuiWindowFlags_AlwaysVerticalScrollbar;
    }
    ImGui::Begin(windowName.c_str(), nullptr, flags);
    ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
    if (pinnedIt->requestFocus) {
      ImGui::SetWindowFocus();
      ImGui::BringWindowToFocusFront(ImGui::GetCurrentWindow());
      pinnedIt->requestFocus = false;
    }
    return PinnableTooltipMode::Pinned;
  }

  if (!a_hoveredSource) {
    return PinnableTooltipMode::None;
  }

  if (manager.stack.empty()) {
    if (!ImGui::BeginTooltip()) {
      return PinnableTooltipMode::None;
    }
    return PinnableTooltipMode::Hovered;
  }

  const auto windowName = "##HoveredTooltip_" + std::string(a_id);
  const auto mousePos = ImGui::GetIO().MousePos;
  ImGui::SetNextWindowPos(ImVec2(mousePos.x + 16.0f, mousePos.y + 16.0f),
                          ImGuiCond_Always);
  if (!ImGui::Begin(windowName.c_str(), nullptr,
                    ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                        ImGuiWindowFlags_NoResize |
                        ImGuiWindowFlags_NoSavedSettings |
                        ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::End();
    return PinnableTooltipMode::None;
  }
  ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
  return PinnableTooltipMode::HoveredOverlay;
}

void EndPinnableTooltip(const std::string_view a_id,
                        const PinnableTooltipMode a_mode) {
  auto &manager = GetPinnedTooltipManager();
  if (a_mode == PinnableTooltipMode::None) {
    return;
  }

  if (a_mode == PinnableTooltipMode::Hovered ||
      a_mode == PinnableTooltipMode::HoveredOverlay) {
    if (manager.altPressedThisFrame) {
      auto pinnedIt = FindPinnedTooltip(manager.stack, a_id);
      if (pinnedIt == manager.stack.end()) {
        PinnedTooltipState state{};
        state.id = std::string(a_id);
        state.pos = ImGui::GetWindowPos();
        state.size = ImGui::GetWindowSize();
        state.hadVerticalScroll = ImGui::GetCurrentWindow()->ScrollMax.y > 0.0f;
        state.requestFocus = true;
        state.renderedThisFrame = true;
        manager.stack.push_back(std::move(state));
      }
    }

    if (a_mode == PinnableTooltipMode::Hovered) {
      ImGui::EndTooltip();
    } else {
      ImGui::End();
    }
    return;
  }

  const auto pinnedIt = FindPinnedTooltip(manager.stack, a_id);
  if (pinnedIt != manager.stack.end()) {
    pinnedIt->renderedThisFrame = true;
    pinnedIt->hoveredThisFrame =
        ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows |
                               ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
  }
  ImGui::End();
}
} // namespace sosr::ui::components
