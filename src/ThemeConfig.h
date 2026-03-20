#pragma once

#include "imgui.h"

#include <nlohmann/json_fwd.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace sosng {
class ThemeConfig {
public:
  static ThemeConfig *GetSingleton();

  bool Load(std::string_view a_themeName);
  void ApplyToImGui() const;

  [[nodiscard]] ImVec4 GetColor(std::string_view a_key,
                                float a_alphaMult = 1.0f) const;
  [[nodiscard]] ImVec4 GetHover(std::string_view a_key,
                                float a_alphaMult = 1.0f) const;
  [[nodiscard]] ImVec4 GetActive(std::string_view a_key,
                                 float a_alphaMult = 1.0f) const;
  [[nodiscard]] ImU32 GetColorU32(std::string_view a_key,
                                  float a_alphaMult = 1.0f) const;
  [[nodiscard]] const std::string &GetThemeName() const { return themeName_; }

private:
  ThemeConfig();

  static void ClampColor(ImVec4 &a_color);
  static std::optional<ImVec4> ParseColor(const nlohmann::json &a_value);
  void LoadDefaultColors();
  [[nodiscard]] std::string BuildThemePath(std::string_view a_themeName) const;

  std::unordered_map<std::string, ImVec4> colors_;
  std::string themeName_{"default"};
  std::string themeDirectory_;
};
} // namespace sosng
