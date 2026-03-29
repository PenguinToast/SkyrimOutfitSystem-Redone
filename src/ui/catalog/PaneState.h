#pragma once

#include "ui/catalog/BrowserState.h"

namespace sosr::ui::catalog {
enum class HostMode : std::uint8_t { Docked, Popout };

struct PaneState {
  BrowserState browser;
  HostMode hostMode{HostMode::Docked};
  bool popoutOpen{false};
};
} // namespace sosr::ui::catalog
