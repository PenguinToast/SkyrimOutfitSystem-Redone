#include "ui/components/PinnableTooltip.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "ui/ThemeConfig.h"

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
  std::size_t currentDepth{0};
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
  manager.currentDepth = 0;

  for (auto &state : manager.stack) {
    state.renderedThisFrame = false;
    state.hoveredThisFrame = false;
  }
}

void EndPinnableTooltipFrame() {
  auto &manager = GetPinnedTooltipManager();
  manager.stack.erase(std::remove_if(manager.stack.begin(), manager.stack.end(),
                                     [](const PinnedTooltipState &a_state) {
                                       return !a_state.renderedThisFrame;
                                     }),
                      manager.stack.end());

  if (!manager.stack.empty() &&
      (ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
       ImGui::IsMouseClicked(ImGuiMouseButton_Right) ||
       ImGui::IsMouseClicked(ImGuiMouseButton_Middle))) {
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
    const auto *theme = sosr::ThemeConfig::GetSingleton();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);
    ImGui::PushStyleColor(ImGuiCol_Border, theme->GetColorU32("PRIMARY"));
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
    ++manager.currentDepth;
    return PinnableTooltipMode::Pinned;
  }

  if (!a_hoveredSource) {
    return PinnableTooltipMode::None;
  }

  if (manager.stack.empty()) {
    const auto &style = ImGui::GetStyle();
    const auto *theme = sosr::ThemeConfig::GetSingleton();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, style.WindowBorderSize);
    ImGui::PushStyleColor(ImGuiCol_Border, theme->GetColorU32("BORDER"));
    if (!ImGui::BeginTooltip()) {
      ImGui::PopStyleColor();
      ImGui::PopStyleVar();
      return PinnableTooltipMode::None;
    }
    ++manager.currentDepth;
    return PinnableTooltipMode::Hovered;
  }

  const auto windowName = "##HoveredTooltip_" + std::string(a_id);
  const auto mousePos = ImGui::GetIO().MousePos;
  const auto &style = ImGui::GetStyle();
  const auto *theme = sosr::ThemeConfig::GetSingleton();
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, style.WindowBorderSize);
  ImGui::PushStyleColor(ImGuiCol_Border, theme->GetColorU32("BORDER"));
  ImGui::SetNextWindowPos(ImVec2(mousePos.x + 16.0f, mousePos.y + 16.0f),
                          ImGuiCond_Always);
  if (!ImGui::Begin(windowName.c_str(), nullptr,
                    ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                        ImGuiWindowFlags_NoResize |
                        ImGuiWindowFlags_NoSavedSettings |
                        ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    return PinnableTooltipMode::None;
  }
  ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
  ++manager.currentDepth;
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
      const auto targetLevel =
          manager.currentDepth > 0 ? manager.currentDepth - 1 : 0;
      if (manager.stack.size() > targetLevel) {
        manager.stack.resize(targetLevel);
      }

      PinnedTooltipState state{};
      state.id = std::string(a_id);
      state.pos = ImGui::GetWindowPos();
      state.size = ImGui::GetWindowSize();
      state.hadVerticalScroll = ImGui::GetCurrentWindow()->ScrollMax.y > 0.0f;
      state.requestFocus = true;
      state.renderedThisFrame = true;
      manager.stack.push_back(std::move(state));
    }

    if (a_mode == PinnableTooltipMode::Hovered) {
      ImGui::EndTooltip();
    } else {
      ImGui::End();
    }
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    if (manager.currentDepth > 0) {
      --manager.currentDepth;
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
  ImGui::PopStyleColor();
  ImGui::PopStyleVar();
  if (manager.currentDepth > 0) {
    --manager.currentDepth;
  }
}
} // namespace sosr::ui::components
