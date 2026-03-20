#pragma once

#include "imgui.h"

#include <cstddef>
#include <string>
#include <vector>

namespace sosr::ui::components {
void DrawTextInputOutline(const ImVec2 &a_min, const ImVec2 &a_max,
                          bool a_hovered, bool a_active,
                          float a_rounding = -1.0f);

bool DrawEditableDropdown(const char *a_label, const char *a_hint,
                          char *a_buffer, std::size_t a_bufferSize,
                          const std::vector<std::string> &a_options,
                          float a_width,
                          std::string *a_selectedOption = nullptr,
                          bool a_acceptAutocompleteOnEnter = true);

bool DrawSearchableStringCombo(const char *a_label, const char *a_allLabel,
                               const std::vector<std::string> &a_options,
                               int &a_index, ImGuiTextFilter &a_filter);
} // namespace sosr::ui::components
