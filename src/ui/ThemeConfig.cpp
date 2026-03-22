#include "ThemeConfig.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace {
constexpr auto kThemeDirectory = "Data/Interface/SkyrimVanitySystem/themes";
constexpr auto kDefaultThemeName = "default";

using ColorEntry = std::pair<std::string_view, ImVec4>;

constexpr std::array<ColorEntry, 28> kDefaultColors{{
    {"NONE", ImVec4(0.00f, 0.00f, 0.00f, 0.00f)},
    {"ERROR",
     ImVec4(218.0f / 255.0f, 79.0f / 255.0f, 79.0f / 255.0f, 200.0f / 255.0f)},
    {"WARN",
     ImVec4(209.0f / 255.0f, 147.0f / 255.0f, 73.0f / 255.0f, 200.0f / 255.0f)},
    {"SUCCESS",
     ImVec4(51.0f / 255.0f, 109.0f / 255.0f, 54.0f / 255.0f, 200.0f / 255.0f)},
    {"CONFIRM",
     ImVec4(90.0f / 255.0f, 138.0f / 255.0f, 92.0f / 255.0f, 200.0f / 255.0f)},
    {"DECLINE",
     ImVec4(200.0f / 255.0f, 69.0f / 255.0f, 69.0f / 255.0f, 200.0f / 255.0f)},
    {"FRAME",
     ImVec4(15.0f / 255.0f, 15.0f / 255.0f, 15.0f / 255.0f, 225.0f / 255.0f)},
    {"BORDER", ImVec4(55.0f / 255.0f, 55.0f / 255.0f, 55.0f / 255.0f, 1.0f)},
    {"TABLE_BG",
     ImVec4(15.0f / 255.0f, 15.0f / 255.0f, 15.0f / 255.0f, 225.0f / 255.0f)},
    {"TABLE_BG_ALT",
     ImVec4(25.0f / 255.0f, 25.0f / 255.0f, 25.0f / 255.0f, 225.0f / 255.0f)},
    {"TABLE_BORDER", ImVec4(0.0f, 0.0f, 0.0f, 0.0f)},
    {"TABLE_SELECTED",
     ImVec4(25.0f / 255.0f, 25.0f / 255.0f, 25.0f / 255.0f, 225.0f / 255.0f)},
    {"TABLE_HOVER",
     ImVec4(200.0f / 255.0f, 200.0f / 255.0f, 200.0f / 255.0f, 1.0f)},
    {"BG",
     ImVec4(15.0f / 255.0f, 15.0f / 255.0f, 15.0f / 255.0f, 225.0f / 255.0f)},
    {"BG_LIGHT",
     ImVec4(35.0f / 255.0f, 35.0f / 255.0f, 35.0f / 255.0f, 225.0f / 255.0f)},
    {"PRIMARY",
     ImVec4(29.0f / 255.0f, 106.0f / 255.0f, 126.0f / 255.0f, 225.0f / 255.0f)},
    {"SECONDARY",
     ImVec4(7.0f / 255.0f, 111.0f / 255.0f, 81.0f / 255.0f, 225.0f / 255.0f)},
    {"TEXT", ImVec4(245.0f / 255.0f, 245.0f / 255.0f, 245.0f / 255.0f, 1.0f)},
    {"TEXT_HEADER",
     ImVec4(66.0f / 255.0f, 172.0f / 255.0f, 200.0f / 255.0f, 1.0f)},
    {"TEXT_DISABLED",
     ImVec4(155.0f / 255.0f, 155.0f / 255.0f, 155.0f / 255.0f, 1.0f)},
    {"SCREEN_BACKGROUND", ImVec4(0.0f, 0.0f, 0.0f, 125.0f / 255.0f)},
    {"WINDOW_BACKGROUND",
     ImVec4(15.0f / 255.0f, 15.0f / 255.0f, 15.0f / 255.0f, 225.0f / 255.0f)},
    {"FILTER_0",
     ImVec4(1.0f, 102.0f / 255.0f, 102.0f / 255.0f, 200.0f / 255.0f)},
    {"FILTER_1", ImVec4(102.0f / 255.0f, 224.0f / 255.0f, 102.0f / 255.0f,
                        200.0f / 255.0f)},
    {"FILTER_2",
     ImVec4(102.0f / 255.0f, 153.0f / 255.0f, 1.0f, 200.0f / 255.0f)},
    {"FILTER_3",
     ImVec4(1.0f, 204.0f / 255.0f, 76.0f / 255.0f, 200.0f / 255.0f)},
    {"FILTER_4",
     ImVec4(204.0f / 255.0f, 102.0f / 255.0f, 1.0f, 200.0f / 255.0f)},
    {"FILTER_5",
     ImVec4(76.0f / 255.0f, 224.0f / 255.0f, 224.0f / 255.0f, 200.0f / 255.0f)},
}};
} // namespace

namespace sosr {
ThemeConfig *ThemeConfig::GetSingleton() {
  static ThemeConfig singleton;
  return std::addressof(singleton);
}

ThemeConfig::ThemeConfig() : themeDirectory_(kThemeDirectory) {
  LoadDefaultColors();
}

bool ThemeConfig::Load(std::string_view a_themeName) {
  LoadDefaultColors();
  themeName_ = a_themeName.empty() ? std::string(kDefaultThemeName)
                                   : std::string(a_themeName);

  const auto path = BuildThemePath(themeName_);
  std::ifstream input(path);
  if (!input.is_open()) {
    logger::warn("Failed to open theme file {}, using defaults", path);
    themeName_ = kDefaultThemeName;
    return false;
  }

  try {
    const auto json = nlohmann::json::parse(input, nullptr, true, true);
    if (json.is_object()) {
      for (const auto &[key, value] : json.items()) {
        if (const auto color = ParseColor(value); color.has_value()) {
          colors_.insert_or_assign(key, *color);
        }
      }
    }
  } catch (const std::exception &exception) {
    logger::warn("Failed to parse theme file {}: {}", path, exception.what());
    themeName_ = kDefaultThemeName;
    LoadDefaultColors();
    return false;
  }

  return true;
}

void ThemeConfig::ApplyToImGui() const {
  auto &style = ImGui::GetStyle();
  style.Colors[ImGuiCol_FrameBg] = GetColor("BG");
  style.Colors[ImGuiCol_FrameBgHovered] = GetHover("BG");
  style.Colors[ImGuiCol_FrameBgActive] = GetActive("BG");

  style.Colors[ImGuiCol_Button] = GetColor("PRIMARY");
  style.Colors[ImGuiCol_ButtonHovered] = GetHover("PRIMARY");
  style.Colors[ImGuiCol_ButtonActive] = GetActive("PRIMARY");

  style.Colors[ImGuiCol_Header] = GetColor("PRIMARY");
  style.Colors[ImGuiCol_HeaderHovered] = GetHover("PRIMARY");
  style.Colors[ImGuiCol_HeaderActive] = GetActive("PRIMARY");

  style.Colors[ImGuiCol_SliderGrab] = GetColor("PRIMARY");
  style.Colors[ImGuiCol_SliderGrabActive] = GetActive("PRIMARY");

  style.Colors[ImGuiCol_ScrollbarBg] = GetColor("BG");
  style.Colors[ImGuiCol_ScrollbarGrab] = GetColor("PRIMARY");
  style.Colors[ImGuiCol_ScrollbarGrabHovered] = GetHover("PRIMARY");
  style.Colors[ImGuiCol_ScrollbarGrabActive] = GetActive("PRIMARY");

  style.Colors[ImGuiCol_Separator] = GetColor("PRIMARY");
  style.Colors[ImGuiCol_SeparatorHovered] = GetHover("PRIMARY");
  style.Colors[ImGuiCol_SeparatorActive] = GetActive("PRIMARY");

  style.Colors[ImGuiCol_ChildBg] = GetColor("NONE");
  style.Colors[ImGuiCol_WindowBg] = GetColor("FRAME");
  style.Colors[ImGuiCol_PopupBg] = GetColor("FRAME");

  style.Colors[ImGuiCol_Text] = GetColor("TEXT");
  style.Colors[ImGuiCol_TextDisabled] = GetColor("TEXT_DISABLED");
  style.Colors[ImGuiCol_Border] = GetColor("BORDER");

  style.Colors[ImGuiCol_TableRowBg] = GetColor("TABLE_BG");
  style.Colors[ImGuiCol_TableRowBgAlt] = GetColor("TABLE_BG_ALT");
  style.Colors[ImGuiCol_TableBorderStrong] = GetColor("TABLE_BORDER");
  style.Colors[ImGuiCol_TableBorderLight] = GetColor("TABLE_BORDER");

  style.Colors[ImGuiCol_TitleBg] = GetColor("FRAME");
  style.Colors[ImGuiCol_TitleBgActive] = GetHover("FRAME");
  style.Colors[ImGuiCol_TitleBgCollapsed] = GetColor("FRAME", 0.90f);
  style.Colors[ImGuiCol_Tab] = GetColor("BG");
  style.Colors[ImGuiCol_TabHovered] = GetHover("PRIMARY");
  style.Colors[ImGuiCol_TabActive] = GetColor("PRIMARY");
  style.Colors[ImGuiCol_TableHeaderBg] = GetColor("BG_LIGHT");
}

ImVec4 ThemeConfig::GetColor(std::string_view a_key, float a_alphaMult) const {
  const auto styleAlpha = ImGui::GetStyle().Alpha;
  if (const auto it = colors_.find(std::string(a_key)); it != colors_.end()) {
    auto color = it->second;
    color.w *= (a_alphaMult * styleAlpha);
    ClampColor(color);
    return color;
  }

  return ImVec4(0.8f, 0.2f, 0.2f, 0.5f * a_alphaMult * styleAlpha);
}

ImVec4 ThemeConfig::GetHover(std::string_view a_key, float a_alphaMult) const {
  auto color = GetColor(a_key, a_alphaMult);
  color.x += 0.05f;
  color.y += 0.05f;
  color.z += 0.10f;
  ClampColor(color);
  return color;
}

ImVec4 ThemeConfig::GetActive(std::string_view a_key, float a_alphaMult) const {
  auto color = GetColor(a_key, a_alphaMult);
  color.x += 0.10f;
  color.y += 0.10f;
  color.z += 0.15f;
  ClampColor(color);
  return color;
}

ImU32 ThemeConfig::GetColorU32(std::string_view a_key,
                               float a_alphaMult) const {
  return ImGui::ColorConvertFloat4ToU32(GetColor(a_key, a_alphaMult));
}

void ThemeConfig::ClampColor(ImVec4 &a_color) {
  a_color.x = std::clamp(a_color.x, 0.0f, 1.0f);
  a_color.y = std::clamp(a_color.y, 0.0f, 1.0f);
  a_color.z = std::clamp(a_color.z, 0.0f, 1.0f);
  a_color.w = std::clamp(a_color.w, 0.0f, 1.0f);
}

std::optional<ImVec4> ThemeConfig::ParseColor(const nlohmann::json &a_value) {
  if (!a_value.is_array() || a_value.size() != 4) {
    return std::nullopt;
  }

  ImVec4 color{};
  bool usesByteRange = false;
  for (std::size_t index = 0; index < 4; ++index) {
    if (!a_value[index].is_number()) {
      return std::nullopt;
    }
    const auto component = a_value[index].get<float>();
    usesByteRange = usesByteRange || component > 1.0f;
    (&color.x)[index] = component;
  }

  if (usesByteRange) {
    color.x /= 255.0f;
    color.y /= 255.0f;
    color.z /= 255.0f;
    color.w /= 255.0f;
  }

  ClampColor(color);
  return color;
}

void ThemeConfig::LoadDefaultColors() {
  colors_.clear();
  colors_.reserve(kDefaultColors.size());
  for (const auto &[key, color] : kDefaultColors) {
    colors_.emplace(std::string(key), color);
  }
}

std::string ThemeConfig::BuildThemePath(std::string_view a_themeName) const {
  const auto theme = a_themeName.empty() ? kDefaultThemeName : a_themeName;
  return (std::filesystem::path(themeDirectory_) /
          (std::string(theme) + ".json"))
      .string();
}
} // namespace sosr
