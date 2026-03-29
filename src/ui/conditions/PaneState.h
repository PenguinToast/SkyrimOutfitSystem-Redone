#pragma once

#include "ui/conditions/EditorState.h"

#include <vector>

namespace sosr::ui::conditions {
struct PaneState {
  int focusedEditorWindowSlot{0};
  std::vector<editor::State> editors;
};
} // namespace sosr::ui::conditions
