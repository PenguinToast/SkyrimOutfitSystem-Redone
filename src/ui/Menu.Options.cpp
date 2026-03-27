#include "Menu.h"

#include "ThemeConfig.h"
#include "backends/imgui_impl_dx11.h"

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <unordered_set>

namespace sosr {
namespace {
constexpr std::array<ImWchar, 3> kLucideIconRanges = {0xE038, 0xE63F, 0};

bool IsFontFile(const std::filesystem::path &a_path) {
  const auto extension = a_path.extension().string();
  if (extension.empty()) {
    return false;
  }

  std::string lowerExtension;
  lowerExtension.reserve(extension.size());
  for (const auto ch : extension) {
    lowerExtension.push_back(static_cast<char>(
        std::tolower(static_cast<unsigned char>(ch))));
  }

  return lowerExtension == ".ttf" || lowerExtension == ".otf" ||
         lowerExtension == ".ttc" || lowerExtension == ".otc";
}

int CompareFontText(std::string_view a_left, std::string_view a_right) {
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

std::string BuildFontLabel(const std::filesystem::path &a_path) {
  if (const auto stem = a_path.stem().string(); !stem.empty()) {
    return stem;
  }

  return a_path.filename().string();
}

void SortFontOptions(std::vector<Menu::FontOption> &a_options) {
  std::ranges::sort(a_options, [](const auto &left, const auto &right) {
    return CompareFontText(left.label, right.label) < 0;
  });
}

std::vector<std::filesystem::path> BuildSystemFontDirectories() {
  std::vector<std::filesystem::path> directories;
  auto getEnvironmentPath = [](const char *name)
      -> std::optional<std::filesystem::path> {
    char *value = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&value, &length, name) != 0 || value == nullptr ||
        length <= 1) {
      free(value);
      return std::nullopt;
    }

    std::filesystem::path result(value);
    free(value);
    return result;
  };

  if (const auto windowsDir = getEnvironmentPath("WINDIR")) {
    directories.emplace_back(*windowsDir / "Fonts");
  } else {
    directories.emplace_back("C:/Windows/Fonts");
  }

  if (const auto localAppData = getEnvironmentPath("LOCALAPPDATA")) {
    directories.emplace_back(*localAppData / "Microsoft/Windows/Fonts");
  }

  std::vector<std::filesystem::path> uniqueDirectories;
  for (const auto &directory : directories) {
    if (std::ranges::find(uniqueDirectories, directory) ==
        uniqueDirectories.end()) {
      uniqueDirectories.push_back(directory);
    }
  }
  return uniqueDirectories;
}

const char *FindSelectedFontLabel(const std::vector<Menu::FontOption> &a_options,
                                  const std::string &a_fontPath) {
  if (const auto it = std::ranges::find_if(
          a_options, [&](const Menu::FontOption &option) {
            return option.path == a_fontPath;
          });
      it != a_options.end()) {
    return it->label.c_str();
  }

  return nullptr;
}
} // namespace

void Menu::RefreshAvailableFonts() {
  bundledFontOptions_.clear();
  systemFontOptions_.clear();

  std::unordered_set<std::string> seenPaths;
  const auto iconFontPath =
      std::filesystem::path(kDefaultIconFontPath).lexically_normal().string();

  auto collectFonts = [&](const std::filesystem::path &directory,
                          std::vector<FontOption> &target, bool isBundled) {
    std::error_code error;
    if (!std::filesystem::exists(directory, error) ||
        !std::filesystem::is_directory(directory, error)) {
      return;
    }

    for (const auto &entry :
         std::filesystem::directory_iterator(directory, error)) {
      if (error || !entry.is_regular_file()) {
        continue;
      }

      const auto &path = entry.path();
      if (!IsFontFile(path)) {
        continue;
      }

      const auto normalizedPath = path.lexically_normal().string();
      if (normalizedPath == iconFontPath ||
          !seenPaths.insert(normalizedPath).second) {
        continue;
      }

      target.push_back({.label = BuildFontLabel(path),
                        .path = normalizedPath,
                        .isBundled = isBundled});
    }
  };

  collectFonts(kBundledFontDirectory, bundledFontOptions_, true);
  for (const auto &directory : BuildSystemFontDirectories()) {
    collectFonts(directory, systemFontOptions_, false);
  }

  SortFontOptions(bundledFontOptions_);
  SortFontOptions(systemFontOptions_);
}

void Menu::NormalizeSelectedFontPath() {
  if (fontPath_.empty()) {
    fontPath_ = kDefaultFontPath;
  }

  auto matchesPath = [&](const FontOption &option) {
    return option.path == fontPath_;
  };

  if (std::ranges::find_if(bundledFontOptions_, matchesPath) !=
          bundledFontOptions_.end() ||
      std::ranges::find_if(systemFontOptions_, matchesPath) !=
          systemFontOptions_.end()) {
    return;
  }

  fontPath_ = kDefaultFontPath;
}

void Menu::RebuildFontAtlas() {
  auto &io = ImGui::GetIO();
  auto &style = ImGui::GetStyle();
  ImFontConfig fontConfig{};
  fontConfig.SizePixels = static_cast<float>(fontSizePixels_);
  fontConfig.OversampleH = 1;
  fontConfig.OversampleV = 1;
  fontConfig.PixelSnapH = true;

  io.Fonts->Clear();
  io.FontDefault = nullptr;
  if (std::filesystem::exists(fontPath_)) {
    io.FontDefault = io.Fonts->AddFontFromFileTTF(
        fontPath_.c_str(), static_cast<float>(fontSizePixels_), &fontConfig);
  } else if (fontPath_ != kDefaultFontPath &&
             std::filesystem::exists(kDefaultFontPath)) {
    io.FontDefault = io.Fonts->AddFontFromFileTTF(
        kDefaultFontPath, static_cast<float>(fontSizePixels_), &fontConfig);
  }
  if (!io.FontDefault) {
    io.FontDefault = io.Fonts->AddFontDefaultVector(&fontConfig);
  }
  if (io.FontDefault && std::filesystem::exists(kDefaultIconFontPath)) {
    ImFontConfig iconConfig{};
    iconConfig.MergeMode = true;
    iconConfig.PixelSnapH = true;
    iconConfig.GlyphOffset.y = 3.0f;
    iconConfig.DstFont = io.FontDefault;
    io.Fonts->AddFontFromFileTTF(kDefaultIconFontPath,
                                 static_cast<float>(fontSizePixels_),
                                 &iconConfig, kLucideIconRanges.data());
  }
  io.Fonts->Build();
  style.FontScaleMain = 1.0f;
  style._NextFrameFontSizeBase = static_cast<float>(fontSizePixels_);
  style.FontSizeBase = static_cast<float>(fontSizePixels_);

  if (initialized_) {
    ImGui_ImplDX11_InvalidateDeviceObjects();
  }
}

void Menu::DrawOptionsTab() {
  ClearCatalogSelection();
  ImGui::TextUnformatted("Interface");
  ImGui::Separator();

  if (ImGui::BeginChild("##options-font-panel", ImVec2(0.0f, 0.0f),
                        ImGuiChildFlags_Borders |
                            ImGuiChildFlags_AutoResizeY)) {
    if (ImGui::BeginTable("##options-layout", 2,
                          ImGuiTableFlags_SizingStretchProp,
                          ImVec2(0.0f, 0.0f))) {
      ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch,
                              1.15f);
      ImGui::TableSetupColumn("Controls", ImGuiTableColumnFlags_WidthStretch,
                              0.85f);
      ImGui::TableNextRow();

      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted("Font");

      ImGui::TableSetColumnIndex(1);
      const char *selectedFontLabel =
          FindSelectedFontLabel(bundledFontOptions_, fontPath_);
      if (!selectedFontLabel) {
        selectedFontLabel = FindSelectedFontLabel(systemFontOptions_, fontPath_);
      }
      if (!selectedFontLabel) {
        selectedFontLabel = "Default";
      }

      ImGui::SetNextItemWidth(-FLT_MIN);
      if (ImGui::BeginCombo("##font-path", selectedFontLabel)) {
        if (!bundledFontOptions_.empty()) {
          ImGui::SeparatorText("Bundled Fonts");
          for (const auto &option : bundledFontOptions_) {
            const bool selected = option.path == fontPath_;
            if (ImGui::Selectable(option.label.c_str(), selected)) {
              fontPath_ = option.path;
              SaveUserSettings();
              pendingFontAtlasRebuild_ = true;
            }
            if (selected) {
              ImGui::SetItemDefaultFocus();
            }
          }
        }

        if (!systemFontOptions_.empty()) {
          ImGui::SeparatorText("System Fonts");
          for (const auto &option : systemFontOptions_) {
            const bool selected = option.path == fontPath_;
            if (ImGui::Selectable(option.label.c_str(), selected)) {
              fontPath_ = option.path;
              SaveUserSettings();
              pendingFontAtlasRebuild_ = true;
            }
            if (selected) {
              ImGui::SetItemDefaultFocus();
            }
          }
        }

        ImGui::EndCombo();
      }

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted("Font Size");
      ImGui::TextDisabled("Range: %d px to %d px", kMinFontSizePixels,
                          kMaxFontSizePixels);

      ImGui::TableSetColumnIndex(1);
      ImGui::SetNextItemWidth(-FLT_MIN);
      ImGui::SliderInt("##font-size", &pendingFontSizePixels_,
                       kMinFontSizePixels, kMaxFontSizePixels, "%d px");
      if (ImGui::IsItemDeactivatedAfterEdit()) {
        fontSizePixels_ = pendingFontSizePixels_;
        SaveUserSettings();
        pendingFontAtlasRebuild_ = true;
      }

      if (ImGui::Button("Reset to Default")) {
        fontPath_ = kDefaultFontPath;
        fontSizePixels_ = kDefaultFontSizePixels;
        pendingFontSizePixels_ = fontSizePixels_;
        SaveUserSettings();
        pendingFontAtlasRebuild_ = true;
      }

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted("Toggle UI Button");

      ImGui::TableSetColumnIndex(1);
      ImGui::SetNextItemWidth(-FLT_MIN);
      if (ImGui::Button(GetToggleKeyLabel().c_str(), ImVec2(-FLT_MIN, 0.0f))) {
        OpenToggleKeyCapture();
      }

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted("Pause Game While Open");

      ImGui::TableSetColumnIndex(1);
      bool pauseGameWhenOpen = pauseGameWhenOpen_;
      if (ImGui::Checkbox("##pause-game-while-open", &pauseGameWhenOpen)) {
        pauseGameWhenOpen_ = pauseGameWhenOpen;
        SaveUserSettings();
      }

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted("Smooth Scrolling");

      ImGui::TableSetColumnIndex(1);
      bool smoothScroll = smoothScroll_;
      if (ImGui::Checkbox("##smooth-scrolling", &smoothScroll)) {
        smoothScroll_ = smoothScroll;
        pendingSmoothWheelDelta_ = 0.0f;
        smoothScrollWindowId_ = 0;
        smoothScrollTargetY_ = 0.0f;
        SaveUserSettings();
      }

      ImGui::EndTable();
    }
    ImGui::EndChild();
  }

  if (openToggleKeyPopup_) {
    ImGui::OpenPopup("##toggle-key-capture");
    openToggleKeyPopup_ = false;
  }

  ImGui::SetNextWindowSize(ImVec2(420.0f, 0.0f), ImGuiCond_Appearing);
  if (ImGui::BeginPopupModal("##toggle-key-capture", nullptr,
                             ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoMove)) {
    ImGui::TextUnformatted("Change Toggle UI Button");
    ImGui::Separator();
    ImGui::TextWrapped(
        "Press the key combination you want to use. Hold Shift, Ctrl, or Alt "
        "while pressing the key to include a modifier.");
    ImGui::TextWrapped(
        "Press T to reset to default (F6). Press Escape to cancel.");
    if (!toggleKeyCaptureError_.empty()) {
      ImGui::Spacing();
      ImGui::TextColored(ThemeConfig::GetSingleton()->GetColor("ERROR"), "%s",
                         toggleKeyCaptureError_.c_str());
    }
    ImGui::Spacing();
    if (ImGui::Button("Cancel", ImVec2(-FLT_MIN, 0.0f))) {
      CloseToggleKeyCapture();
      ImGui::CloseCurrentPopup();
    }
    if (!awaitingToggleKeyCapture_) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}
} // namespace sosr
