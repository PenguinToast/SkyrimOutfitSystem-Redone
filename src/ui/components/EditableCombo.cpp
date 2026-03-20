#include "ui/components/EditableCombo.h"

#include "InputManager.h"
#include "imgui_internal.h"
#include "ui/ThemeConfig.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string_view>

namespace {
std::string TrimText(std::string_view a_text) {
  std::size_t start = 0;
  while (start < a_text.size() &&
         std::isspace(static_cast<unsigned char>(a_text[start])) != 0) {
    ++start;
  }

  std::size_t end = a_text.size();
  while (end > start &&
         std::isspace(static_cast<unsigned char>(a_text[end - 1])) != 0) {
    --end;
  }

  return std::string(a_text.substr(start, end - start));
}

bool ContainsCaseInsensitive(std::string_view a_haystack,
                             std::string_view a_needle) {
  if (a_needle.empty()) {
    return true;
  }

  const auto lower = [](const unsigned char a_value) {
    return static_cast<char>(std::tolower(a_value));
  };

  for (std::size_t index = 0; index + a_needle.size() <= a_haystack.size();
       ++index) {
    bool matches = true;
    for (std::size_t offset = 0; offset < a_needle.size(); ++offset) {
      if (lower(static_cast<unsigned char>(a_haystack[index + offset])) !=
          lower(static_cast<unsigned char>(a_needle[offset]))) {
        matches = false;
        break;
      }
    }
    if (matches) {
      return true;
    }
  }

  return false;
}

int CompareText(std::string_view a_left, std::string_view a_right) {
  const auto leftSize = a_left.size();
  const auto rightSize = a_right.size();
  const auto count = (std::min)(leftSize, rightSize);

  for (std::size_t index = 0; index < count; ++index) {
    const auto left = static_cast<unsigned char>(
        std::tolower(static_cast<unsigned char>(a_left[index])));
    const auto right = static_cast<unsigned char>(
        std::tolower(static_cast<unsigned char>(a_right[index])));
    if (left < right) {
      return -1;
    }
    if (left > right) {
      return 1;
    }
  }

  if (leftSize < rightSize) {
    return -1;
  }
  if (leftSize > rightSize) {
    return 1;
  }
  return 0;
}

const std::string *
FindTopAutocompleteOption(const std::vector<std::string> &a_options,
                          std::string_view a_buffer) {
  const auto needle = TrimText(a_buffer);
  for (const auto &option : a_options) {
    if (!needle.empty() && !ContainsCaseInsensitive(option, needle)) {
      continue;
    }
    return std::addressof(option);
  }
  return nullptr;
}

struct EditableDropdownAutocompleteData {
  const std::vector<std::string> *options{nullptr};
};

int EditableDropdownInputCallback(ImGuiInputTextCallbackData *a_data) {
  auto *userData =
      static_cast<EditableDropdownAutocompleteData *>(a_data->UserData);
  if (!userData || !userData->options ||
      a_data->EventFlag != ImGuiInputTextFlags_CallbackCompletion) {
    return 0;
  }

  const std::string_view buffer{a_data->Buf,
                                static_cast<std::size_t>(a_data->BufTextLen)};
  const auto *topOption = FindTopAutocompleteOption(*userData->options, buffer);
  if (!topOption) {
    return 0;
  }

  a_data->DeleteChars(0, a_data->BufTextLen);
  a_data->InsertChars(0, topOption->c_str());
  a_data->SelectAll();
  return 0;
}
} // namespace

namespace sosr::ui::components {
void DrawTextInputOutline(const ImVec2 &a_min, const ImVec2 &a_max,
                          const bool a_hovered, const bool a_active,
                          const float a_rounding) {
  if (!a_hovered && !a_active) {
    return;
  }

  auto *theme = ThemeConfig::GetSingleton();
  auto *drawList = ImGui::GetWindowDrawList();
  const auto rounding =
      a_rounding >= 0.0f ? a_rounding : ImGui::GetStyle().FrameRounding;
  const auto color = a_active ? theme->GetColorU32("PRIMARY")
                              : theme->GetColorU32("TABLE_HOVER", 0.75f);
  const auto thickness = a_active ? 2.0f : 1.5f;
  drawList->AddRect(a_min, a_max, color, rounding, 0, thickness);
}

bool DrawSearchableStringCombo(const char *a_label, const char *a_allLabel,
                               const std::vector<std::string> &a_options,
                               int &a_index, ImGuiTextFilter &a_filter) {
  const char *preview =
      a_index == 0 ? a_allLabel
                   : a_options[static_cast<std::size_t>(a_index - 1)].c_str();
  const float width = ImGui::CalcItemWidth();

  std::vector<std::string> options;
  options.reserve(a_options.size() + 1);
  options.emplace_back(a_allLabel);
  options.insert(options.end(), a_options.begin(), a_options.end());

  const bool changed =
      DrawEditableDropdown(a_label, preview, a_filter.InputBuf,
                           IM_ARRAYSIZE(a_filter.InputBuf), options, width);

  if (a_filter.InputBuf[0] == '\0') {
    a_index = 0;
    a_filter.Build();
    return changed;
  }

  for (std::size_t index = 0; index < options.size(); ++index) {
    if (CompareText(options[index], a_filter.InputBuf) != 0) {
      continue;
    }

    a_index = static_cast<int>(index);
    a_filter.Build();
    return true;
  }

  a_filter.Build();
  return changed;
}

bool DrawEditableDropdown(const char *a_label, const char *a_hint,
                          char *a_buffer, const std::size_t a_bufferSize,
                          const std::vector<std::string> &a_options,
                          const float a_width, std::string *a_selectedOption,
                          const bool a_acceptAutocompleteOnEnter) {
  bool changed = false;
  const auto popupId = std::string(a_label) + "##popup";
  const auto openId = std::string(a_label) + "##open";
  const auto buttonWidth = ImGui::GetFrameHeight();
  const auto inputWidth = (std::max)(1.0f, a_width - buttonWidth);
  EditableDropdownAutocompleteData autocompleteData{std::addressof(a_options)};
  ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_AutoSelectAll |
                                   ImGuiInputTextFlags_CallbackCompletion;
  if (a_acceptAutocompleteOnEnter) {
    inputFlags |= ImGuiInputTextFlags_EnterReturnsTrue;
  }

  ImGui::PushID(a_label);
  auto *storage = ImGui::GetStateStorage();
  const auto openStorageId = ImGui::GetID(openId.c_str());
  const auto popupImGuiId = ImGui::GetID(popupId.c_str());

  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
  ImGui::SetNextItemWidth(inputWidth);
  const bool submitted = ImGui::InputTextWithHint(
      "##input", a_hint, a_buffer, a_bufferSize, inputFlags,
      EditableDropdownInputCallback, std::addressof(autocompleteData));
  if (submitted) {
    changed = true;
  }
  const bool inputTextActive = ImGui::IsItemActive();
  if (ImGui::IsItemActivated()) {
    InputManager::GetSingleton()->Flush();
    storage->SetBool(openStorageId, true);
    ImGui::OpenPopup(popupId.c_str());
  }
  if (inputTextActive) {
    storage->SetBool(openStorageId, true);
  }

  const auto inputMin = ImGui::GetItemRectMin();
  const auto inputMax = ImGui::GetItemRectMax();
  const auto inputFrameHeight = inputMax.y - inputMin.y;
  const bool inputHovered = ImGui::IsItemHovered();
  ImGui::SameLine(0.0f, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
  if (ImGui::ArrowButton("##open", ImGuiDir_Down)) {
    storage->SetBool(openStorageId, true);
    ImGui::OpenPopup(popupId.c_str());
  }
  ImGui::PopStyleVar();
  const bool arrowHovered = ImGui::IsItemHovered();
  const bool arrowPressed = ImGui::IsItemActivated();
  ImGui::PopStyleVar();
  bool dropdownOpen = storage->GetBool(openStorageId, false);
  if (arrowPressed) {
    dropdownOpen = true;
  }

  if (dropdownOpen) {
    ImGui::SetNextWindowPos(
        ImVec2(inputMin.x, inputMax.y + ImGui::GetStyle().FramePadding.y));
    ImGui::SetNextWindowSizeConstraints(
        ImVec2(a_width, 0.0f),
        ImVec2(a_width, ImGui::GetTextLineHeightWithSpacing() * 12.0f));
    ImGuiWindowFlags popupFlags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav;
    bool popupVisible = false;
    bool popupHovered = false;
    if (ImGui::BeginPopupEx(popupImGuiId, popupFlags)) {
      popupVisible = true;
      ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
      popupHovered = ImGui::IsWindowHovered(
          ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

      const auto needle = TrimText(a_buffer);
      bool anyVisible = false;
      const std::string *topOption =
          FindTopAutocompleteOption(a_options, needle);
      const auto commitOption = [&](const std::string &a_option) {
        std::snprintf(a_buffer, a_bufferSize, "%s", a_option.c_str());
        if (a_selectedOption) {
          *a_selectedOption = a_option;
        }
        changed = true;
        dropdownOpen = false;
        ImGui::CloseCurrentPopup();
      };
      for (const auto &option : a_options) {
        if (!needle.empty() && !ContainsCaseInsensitive(option, needle)) {
          continue;
        }

        anyVisible = true;
        const bool selected = topOption == std::addressof(option);
        if (ImGui::Selectable(option.c_str(), selected)) {
          commitOption(option);
          ImGui::SetKeyboardFocusHere(-1);
        }
        if (selected) {
          ImGui::SetItemDefaultFocus();
        }
      }

      if (submitted && a_acceptAutocompleteOnEnter && topOption) {
        commitOption(*topOption);
        ImGui::SetKeyboardFocusHere(-1);
      }

      if (!anyVisible) {
        ImGui::TextDisabled("No matches");
      }
      ImGui::EndPopup();
    }

    if (!popupVisible || (!inputTextActive && !arrowHovered && !popupHovered)) {
      dropdownOpen = false;
    }
  }

  auto *drawList = ImGui::GetWindowDrawList();
  const auto arrowMin = ImGui::GetItemRectMin();
  const auto arrowMax = ImGui::GetItemRectMax();
  DrawTextInputOutline(inputMin, arrowMax, inputHovered || arrowHovered,
                       inputTextActive, ImGui::GetStyle().FrameRounding);
  drawList->AddLine(ImVec2(arrowMin.x, inputMin.y + 1.0f),
                    ImVec2(arrowMin.x, inputMin.y + inputFrameHeight - 1.0f),
                    ThemeConfig::GetSingleton()->GetColorU32("BORDER"));

  storage->SetBool(openStorageId, dropdownOpen);
  ImGui::PopID();
  return changed;
}
} // namespace sosr::ui::components
