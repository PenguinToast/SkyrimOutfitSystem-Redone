#pragma once

#include <cstdint>
#include <string>

namespace sosr::keycode {
[[nodiscard]] bool IsKeyModifier(std::uint32_t a_key);
[[nodiscard]] bool IsValidHotkey(std::uint32_t a_key);
[[nodiscard]] std::string GetKeyName(std::uint32_t a_scanCode);
} // namespace sosr::keycode
